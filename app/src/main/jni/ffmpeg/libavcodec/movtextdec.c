/*
 * 3GPP TS 26.245 Timed Text decoder
 * Copyright (c) 2012  Philip Langdale <philipl@overt.org>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "avcodec.h"
#include "ass.h"
#include "libavutil/avstring.h"
#include "libavutil/common.h"
#include "libavutil/bprint.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/mem.h"

#define STYLE_FLAG_BOLD         (1<<0)
#define STYLE_FLAG_ITALIC       (1<<1)
#define STYLE_FLAG_UNDERLINE    (1<<2)

#define STYL_BOX   (1<<0)
#define HLIT_BOX   (1<<1)
#define HCLR_BOX   (1<<2)

typedef struct {
    uint16_t style_start;
    uint16_t style_end;
    uint8_t style_flag;
} StyleBox;

typedef struct {
    uint16_t hlit_start;
    uint16_t hlit_end;
} HighlightBox;

typedef struct {
   uint8_t hlit_color[4];
} HilightcolorBox;

typedef struct {
    StyleBox **s;
    StyleBox *s_temp;
    HighlightBox h;
    HilightcolorBox c;
    uint8_t box_flags;
    uint16_t style_entries;
    uint64_t tracksize;
    int size_var;
    int count_s;
} MovTextContext;

typedef struct {
    uint32_t type;
    size_t base_size;
    int (*decode)(const uint8_t *tsmb, MovTextContext *m, AVPacket *avpkt);
} Box;

static void mov_text_cleanup(MovTextContext *m)
{
    int i;
    if (m->box_flags & STYL_BOX) {
        for(i = 0; i < m->count_s; i++) {
            av_freep(&m->s[i]);
        }
        av_freep(&m->s);
    }
}

static int decode_hlit(const uint8_t *tsmb, MovTextContext *m, AVPacket *avpkt)
{
    m->box_flags |= HLIT_BOX;
    m->h.hlit_start = AV_RB16(tsmb);
    tsmb += 2;
    m->h.hlit_end = AV_RB16(tsmb);
    tsmb += 2;
    return 0;
}

static int decode_hclr(const uint8_t *tsmb, MovTextContext *m, AVPacket *avpkt)
{
    m->box_flags |= HCLR_BOX;
    memcpy(m->c.hlit_color, tsmb, 4);
    tsmb += 4;
    return 0;
}

static int decode_styl(const uint8_t *tsmb, MovTextContext *m, AVPacket *avpkt)
{
    int i;
    m->style_entries = AV_RB16(tsmb);
    tsmb += 2;
    // A single style record is of length 12 bytes.
    if (m->tracksize + m->size_var + 2 + m->style_entries * 12 > avpkt->size)
        return -1;

    m->box_flags |= STYL_BOX;
    for(i = 0; i < m->style_entries; i++) {
        m->s_temp = av_malloc(sizeof(*m->s_temp));
        if (!m->s_temp) {
            mov_text_cleanup(m);
            return AVERROR(ENOMEM);
        }
        m->s_temp->style_start = AV_RB16(tsmb);
        tsmb += 2;
        m->s_temp->style_end = AV_RB16(tsmb);
        tsmb += 2;
        // fontID = AV_RB16(tsmb);
        tsmb += 2;
        m->s_temp->style_flag = AV_RB8(tsmb);
        av_dynarray_add(&m->s, &m->count_s, m->s_temp);
        if(!m->s) {
            mov_text_cleanup(m);
            return AVERROR(ENOMEM);
        }
        // fontsize = AV_RB8(tsmb);
        tsmb += 2;
        // text-color-rgba
        tsmb += 4;
    }
    return 0;
}

static const Box box_types[] = {
    { MKBETAG('s','t','y','l'), 2, decode_styl },
    { MKBETAG('h','l','i','t'), 4, decode_hlit },
    { MKBETAG('h','c','l','r'), 4, decode_hclr }
};

const static size_t box_count = FF_ARRAY_ELEMS(box_types);

static int text_to_ass(AVBPrint *buf, const char *text, const char *text_end,
                        MovTextContext *m)
{
    int i = 0;
    int text_pos = 0;
    while (text < text_end) {
        if (m->box_flags & STYL_BOX) {
            for (i = 0; i < m->style_entries; i++) {
                if (m->s[i]->style_flag && text_pos == m->s[i]->style_end) {
                    if (m->s[i]->style_flag & STYLE_FLAG_BOLD)
                        av_bprintf(buf, "{\\b0}");
                    if (m->s[i]->style_flag & STYLE_FLAG_ITALIC)
                        av_bprintf(buf, "{\\i0}");
                    if (m->s[i]->style_flag & STYLE_FLAG_UNDERLINE)
                        av_bprintf(buf, "{\\u0}");
                }
            }
            for (i = 0; i < m->style_entries; i++) {
                if (m->s[i]->style_flag && text_pos == m->s[i]->style_start) {
                    if (m->s[i]->style_flag & STYLE_FLAG_BOLD)
                        av_bprintf(buf, "{\\b1}");
                    if (m->s[i]->style_flag & STYLE_FLAG_ITALIC)
                        av_bprintf(buf, "{\\i1}");
                    if (m->s[i]->style_flag & STYLE_FLAG_UNDERLINE)
                        av_bprintf(buf, "{\\u1}");
                }
            }
        }
        if (m->box_flags & HLIT_BOX) {
            if (text_pos == m->h.hlit_start) {
                /* If hclr box is present, set the secondary color to the color
                 * specified. Otherwise, set primary color to white and secondary
                 * color to black. These colors will come from TextSampleModifier
                 * boxes in future and inverse video technique for highlight will
                 * be implemented.
                 */
                if (m->box_flags & HCLR_BOX) {
                    av_bprintf(buf, "{\\2c&H%02x%02x%02x&}", m->c.hlit_color[2],
                                m->c.hlit_color[1], m->c.hlit_color[0]);
                } else {
                    av_bprintf(buf, "{\\1c&H000000&}{\\2c&HFFFFFF}");
                }
            }
            if (text_pos == m->h.hlit_end) {
                if (m->box_flags & HCLR_BOX) {
                    av_bprintf(buf, "{\\2c&H000000}");
                } else {
                    av_bprintf(buf, "{\\1c&HFFFFFF&}{\\2c&H000000}");
                }
            }
        }

        switch (*text) {
        case '\r':
            break;
        case '\n':
            av_bprintf(buf, "\\N");
            break;
        default:
            av_bprint_chars(buf, *text, 1);
            break;
        }
        text++;
        text_pos++;
    }

    return 0;
}

static int mov_text_init(AVCodecContext *avctx) {
    /*
     * TODO: Handle the default text style.
     * NB: Most players ignore styles completely, with the result that
     * it's very common to find files where the default style is broken
     * and respecting it results in a worse experience than ignoring it.
     */
    return ff_ass_subtitle_header_default(avctx);
}

static int mov_text_decode_frame(AVCodecContext *avctx,
                            void *data, int *got_sub_ptr, AVPacket *avpkt)
{
    AVSubtitle *sub = data;
    MovTextContext *m = avctx->priv_data;
    int ret, ts_start, ts_end;
    AVBPrint buf;
    char *ptr = avpkt->data;
    char *end;
    int text_length, tsmb_type, ret_tsmb;
    uint64_t tsmb_size;
    const uint8_t *tsmb;

    if (!ptr || avpkt->size < 2)
        return AVERROR_INVALIDDATA;

    /*
     * A packet of size two with value zero is an empty subtitle
     * used to mark the end of the previous non-empty subtitle.
     * We can just drop them here as we have duration information
     * already. If the value is non-zero, then it's technically a
     * bad packet.
     */
    if (avpkt->size == 2)
        return AV_RB16(ptr) == 0 ? 0 : AVERROR_INVALIDDATA;

    /*
     * The first two bytes of the packet are the length of the text string
     * In complex cases, there are style descriptors appended to the string
     * so we can't just assume the packet size is the string size.
     */
    text_length = AV_RB16(ptr);
    end = ptr + FFMIN(2 + text_length, avpkt->size);
    ptr += 2;

    ts_start = av_rescale_q(avpkt->pts,
                            avctx->time_base,
                            (AVRational){1,100});
    ts_end   = av_rescale_q(avpkt->pts + avpkt->duration,
                            avctx->time_base,
                            (AVRational){1,100});

    tsmb_size = 0;
    m->tracksize = 2 + text_length;
    m->style_entries = 0;
    m->box_flags = 0;
    m->count_s = 0;
    // Note that the spec recommends lines be no longer than 2048 characters.
    av_bprint_init(&buf, 0, AV_BPRINT_SIZE_UNLIMITED);
    if (text_length + 2 != avpkt->size) {
        while (m->tracksize + 8 <= avpkt->size) {
            // A box is a minimum of 8 bytes.
            tsmb = ptr + m->tracksize - 2;
            tsmb_size = AV_RB32(tsmb);
            tsmb += 4;
            tsmb_type = AV_RB32(tsmb);
            tsmb += 4;

            if (tsmb_size == 1) {
                if (m->tracksize + 16 > avpkt->size)
                    break;
                tsmb_size = AV_RB64(tsmb);
                tsmb += 8;
                m->size_var = 16;
            } else
                m->size_var = 8;
            //size_var is equal to 8 or 16 depending on the size of box

            if (m->tracksize + tsmb_size > avpkt->size)
                break;

            for (size_t i = 0; i < box_count; i++) {
                if (tsmb_type == box_types[i].type) {
                    if (m->tracksize + m->size_var + box_types[i].base_size > avpkt->size)
                        break;
                    ret_tsmb = box_types[i].decode(tsmb, m, avpkt);
                    if (ret_tsmb == -1)
                        break;
                }
            }
            m->tracksize = m->tracksize + tsmb_size;
        }
        text_to_ass(&buf, ptr, end, m);
        mov_text_cleanup(m);
    } else
        text_to_ass(&buf, ptr, end, m);

    ret = ff_ass_add_rect_bprint(sub, &buf, ts_start, ts_end - ts_start);
    av_bprint_finalize(&buf, NULL);
    if (ret < 0)
        return ret;
    *got_sub_ptr = sub->num_rects > 0;
    return avpkt->size;
}

AVCodec ff_movtext_decoder = {
    .name         = "mov_text",
    .long_name    = NULL_IF_CONFIG_SMALL("3GPP Timed Text subtitle"),
    .type         = AVMEDIA_TYPE_SUBTITLE,
    .id           = AV_CODEC_ID_MOV_TEXT,
    .priv_data_size = sizeof(MovTextContext),
    .init         = mov_text_init,
    .decode       = mov_text_decode_frame,
};

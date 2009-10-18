#include <string.h>
#include <glib.h>
#include "himd.h"

#include "himd_private.h"

#define _(x) (x)

static unsigned char * get_track(struct himd * himd, unsigned int idx)
{
    return himd->tifdata +  0x8000 + 0x50 * idx;
}

static unsigned char * get_frag(struct himd * himd, unsigned int idx)
{
    return himd->tifdata + 0x30000 + 0x10 * idx;
}

static unsigned char * get_strchunk(struct himd * himd, unsigned int idx)
{
    return himd->tifdata + 0x40000 + 0x10 * idx;
}

static int strtype(unsigned char * stringchunk)
{
    return stringchunk[14] >> 4;
}

static int strlink(unsigned char * stringchunk)
{
    return beword16(stringchunk+14) & 0xFFF;
}

unsigned int himd_track_count(struct himd * himd)
{
    return beword16(himd->tifdata + 0x100);
}

unsigned int himd_get_trackslot(struct himd * himd, unsigned int idx, struct himderrinfo * status)
{
    if(idx >= himd_track_count(himd))
    {
        set_status_printf(status, HIMD_ERROR_NO_SUCH_TRACK, _("Track %d of %d requested"));
        return 0;
    }
    return beword16(himd->tifdata + 0x102 + 2*idx);
}


int himd_get_track_info(struct himd * himd, unsigned int idx, struct trackinfo * t, struct himderrinfo * status)
{
    unsigned char * trackbuffer;
    unsigned int firstpart;

    g_return_val_if_fail(himd != NULL, -1);
    g_return_val_if_fail(idx >= HIMD_FIRST_TRACK, -1);
    g_return_val_if_fail(idx <= HIMD_LAST_TRACK, -1);
    g_return_val_if_fail(t != NULL, -1);

    trackbuffer = get_track(himd, idx);
    firstpart = beword16(trackbuffer+36);

    if(firstpart == 0)
    {
        set_status_printf(status, HIMD_ERROR_NO_SUCH_TRACK,
                          _("Track %d is not present on disc"), idx);
        return -1;
    }
    t->ekbnum = beword32(trackbuffer+4);
    t->title = beword16(trackbuffer+8);
    t->artist = beword16(trackbuffer+10);
    t->album = beword16(trackbuffer+12);
    t->trackinalbum = trackbuffer[14];
    memcpy(t->key, trackbuffer+16,8);
    memcpy(t->mac, trackbuffer+24,8);
    t->codec_id = trackbuffer[32];
    memcpy(t->codecinfo,trackbuffer+33,3);
    memcpy(t->codecinfo+3,trackbuffer+44,2);
    t->firstfrag = firstpart;
    t->tracknum = beword16(trackbuffer+38);
    t->seconds = beword16(trackbuffer+40);
    memcpy(t->contentid,trackbuffer+48,20);
    return 0;
}

const char * himd_get_codec_name(const struct trackinfo * track)
{
    static char buffer[5];

    g_return_val_if_fail(track != NULL, "(nullptr)");

    if(track->codec_id == CODEC_LPCM)
        return "LPCM";
    if(track->codec_id == CODEC_ATRAC3)
        return "AT3 ";
    if(track->codec_id == CODEC_ATRAC3PLUS_OR_MPEG && 
         (track->codecinfo[0] & 3) == 0)
        return "AT3+";
    if(track->codec_id == CODEC_ATRAC3PLUS_OR_MPEG)
        return "MPEG";
    sprintf(buffer,"%4d",track->codec_id);
    return buffer;
}

unsigned int himd_trackinfo_framesize(const struct trackinfo * track)
{
    g_return_val_if_fail(track != NULL, 0);

    if(track->codec_id == CODEC_LPCM)
        return HIMD_LPCM_FRAMESIZE;
    if(track->codec_id == CODEC_ATRAC3)
        return 8 * track->codecinfo[2];
    if(track->codec_id == CODEC_ATRAC3PLUS_OR_MPEG &&
         (track->codecinfo[0] & 3) == 0)
        return 8 * (track->codecinfo[2] + 1);
    /* MP3 tracks don't have a fixed frame size, other track types unknown */
    return 0;
}

unsigned int himd_trackinfo_framesperblock(const struct trackinfo * track)
{
    int framesize;

    g_return_val_if_fail(track != NULL, 0);

    framesize = himd_trackinfo_framesize(track);
    if(!framesize)
        return TRACK_IS_MPEG;

    if(track->codec_id == CODEC_LPCM)
        return 0x3FC0 / HIMD_LPCM_FRAMESIZE;
    else
        return 0x3FBF / framesize;

    /* other track types unknown */
    return 0;
}

int himd_track_uploadable(struct himd * himd, const struct trackinfo * track)
{
    struct fraginfo frag;
    int fragnum;

    g_return_val_if_fail(himd != NULL, 0);
    g_return_val_if_fail(track != NULL, 0);

    /* MPEG has no serious encryption */
    if(track->codec_id == CODEC_ATRAC3PLUS_OR_MPEG &&
       (track->codecinfo[0] & 3) == 3)
        return 1;

    /* Not the well-known RH1 key */
    if(memcmp(track->key,"\0\0\0\0\0\0\0",8) != 0 ||
       track->ekbnum != 0x10012)
        return 0;

    for(fragnum = track->firstfrag; fragnum != 0; fragnum = frag.nextfrag)
    {
        if(himd_get_fragment_info(himd, fragnum, &frag, NULL) < 0)
            return 0;
        if(memcmp(frag.key,"\0\0\0\0\0\0\0",8) != 0)
            return 0;
    }
    return 1;
}

int himd_track_blocks(struct himd * himd, const struct trackinfo * track, struct himderrinfo * status)
{
    struct fraginfo frag;
    int fragnum, blocks = 0;

    g_return_val_if_fail(himd != NULL, -1);
    g_return_val_if_fail(track != NULL, -1);

    for(fragnum = track->firstfrag; fragnum != 0; fragnum = frag.nextfrag)
    {
        if(himd_get_fragment_info(himd, fragnum, &frag, status) < 0)
            return -1;
        blocks += frag.lastblock - frag.firstblock + 1;
    }
    return blocks;
}

int himd_get_fragment_info(struct himd * himd, unsigned int idx, struct fraginfo * f, struct himderrinfo * status)
{
    unsigned char * fragbuffer;

    g_return_val_if_fail(himd != NULL, -1);
    g_return_val_if_fail(idx >= HIMD_FIRST_TRACK, -1);
    g_return_val_if_fail(idx <= HIMD_LAST_TRACK, -1);
    g_return_val_if_fail(f != NULL, -1);

    fragbuffer = get_frag(himd, idx);
    memcpy(f->key, fragbuffer, 8);
    f->firstblock = beword16(fragbuffer + 8);
    f->lastblock = beword16(fragbuffer + 10);
    f->firstframe = fragbuffer[12];
    f->lastframe = fragbuffer[13];
    f->fragtype = fragbuffer[14] >> 4;
    f->nextfrag = beword16(fragbuffer+14) & 0xFFF;
    (void)status;
    return 0;
}

char* himd_get_string_raw(struct himd * himd, unsigned int idx, int*type, int* length, struct himderrinfo * status)
{
    int curidx;
    int len;
    char * rawstr;
    int actualtype;

    g_return_val_if_fail(himd != NULL, NULL);
    g_return_val_if_fail(idx >= 1, NULL);
    g_return_val_if_fail(idx < 4096, NULL);
    
    actualtype = strtype(get_strchunk(himd,idx));
    /* Not the head of a string */
    if(actualtype < 8)
    {
        set_status_printf(status, HIMD_ERROR_NOT_STRING_HEAD,
                   _("String table entry %d is not a head: Type %d"),
                   idx,actualtype);
        return NULL;
    }
    if(type != NULL)
        *type = actualtype;

    /* Get length of string */
    len = 1;
    for(curidx = strlink(get_strchunk(himd,idx)); curidx != 0; 
          curidx = strlink(get_strchunk(himd,curidx)))
    {
        if(strtype(get_strchunk(himd,curidx)) != STRING_TYPE_CONTINUATION)
        {
            set_status_printf(status, HIMD_ERROR_STRING_CHAIN_BROKEN,
                       _("%dth entry in string chain starting at %d has type %d"),
                       len+1,idx,strtype(get_strchunk(himd,curidx)));
            return NULL;
        }
        len++;
        if(len >= 4096)
        {
            set_status_printf(status, HIMD_ERROR_STRING_CHAIN_BROKEN,
                       _("string chain starting at %d loops"),idx);
            return NULL;
        }
    }

    /* collect fragments */
    rawstr = g_malloc(len*14);
    if(!rawstr)
    {
        set_status_printf(status, HIMD_ERROR_OUT_OF_MEMORY,
                   _("Can't allocate %d bytes for raw string (string idx %d)"),
                   len, idx);
        return NULL;
    }

    len = 0;
    for(curidx = idx; curidx != 0; 
          curidx = strlink(get_strchunk(himd,curidx)))
    {
        memcpy(rawstr+len*14,get_strchunk(himd,curidx),14);
        len++;
    }

    *length = 14*len;
    return rawstr;
}

char* himd_get_string_utf8(struct himd * himd, unsigned int idx, int*type, struct himderrinfo * status)
{
    int length;
    char * out;
    char * srcencoding;
    char * rawstr;
    GError * err = NULL;
    g_return_val_if_fail(himd != NULL, NULL);
    g_return_val_if_fail(idx >= 1, NULL);
    g_return_val_if_fail(idx < 4096, NULL);
    
    rawstr = himd_get_string_raw(himd, idx, type, &length, status);
    if(!rawstr)
        return NULL;

    switch((unsigned char)rawstr[0])
    {
        case HIMD_ENCODING_LATIN1:
            srcencoding = "ISO-8859-1";
            break;
        case HIMD_ENCODING_UTF16BE:
            srcencoding = "UTF-16BE";
            break;
        case HIMD_ENCODING_SHIFT_JIS:
            srcencoding = "SHIFT_JIS";
            break;
        default:
            set_status_printf(status, HIMD_ERROR_UNKNOWN_ENCODING,
                       "string %d has unknown encoding with ID %d",
                       idx, rawstr[0]);
            himd_free(rawstr);
            return NULL;
    }
    out = g_convert(rawstr+1,length-1,"UTF-8",srcencoding,NULL,NULL,&err);
    himd_free(rawstr);
    if(err)
    {
        set_status_printf(status, HIMD_ERROR_STRING_ENCODING_ERROR,
                   "convert string %d from %s to UTF-8: %s",
                   idx, srcencoding, err->message);
        return NULL;
    }
    return out;
}

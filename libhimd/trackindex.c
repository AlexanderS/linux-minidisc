#include <string.h>
#include <glib.h>
#include "himd.h"

#define _(x) (x)

static unsigned int beword16(unsigned char * c)
{
    return c[0]*256+c[1];
}

static unsigned char * get_track(struct himd * himd, unsigned int idx)
{
    return himd->tifdata +  0x8000 + 0x50 * idx;
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

int himd_get_track_info(struct himd * himd, unsigned int idx, struct trackinfo * t)
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
        himd->status = HIMD_ERROR_NO_SUCH_TRACK;
        g_snprintf(himd->statusmsg, sizeof himd->statusmsg, _("Track %d is not present on disc"), idx);
        return -1;
    }
    t->title = beword16(trackbuffer+8);
    t->artist = beword16(trackbuffer+10);
    t->album = beword16(trackbuffer+12);
    t->trackinalbum = trackbuffer[14];
    t->codec_id = trackbuffer[32];
    memcpy(t->codecinfo,trackbuffer+33,3);
    memcpy(t->codecinfo+3,trackbuffer+44,2);
    t->firstpart = firstpart;
    t->tracknum = beword16(trackbuffer+38);
    t->seconds = beword16(trackbuffer+40);
    return 0;
}

char* himd_get_string_raw(struct himd * himd, unsigned int idx, int*type, int* length)
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
        himd->status = HIMD_ERROR_NOT_STRING_HEAD;
        g_snprintf(himd->statusmsg,sizeof(himd->statusmsg),
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
            himd->status = HIMD_ERROR_STRING_CHAIN_BROKEN;
            g_snprintf(himd->statusmsg,sizeof(himd->statusmsg),
                       "%dth entry in string chain starting at %d has type %d",
                       len+1,idx,strtype(get_strchunk(himd,curidx)));
            return NULL;
        }
        len++;
        if(len >= 4096)
        {
            himd->status = HIMD_ERROR_STRING_CHAIN_BROKEN;
            g_snprintf(himd->statusmsg,sizeof(himd->statusmsg),
                       "string chain starting at %d loops",idx);
            return NULL;
        }
    }

    /* collect fragments */
    rawstr = g_malloc(len*14);
    if(!rawstr)
    {
        himd->status = HIMD_ERROR_OUT_OF_MEMORY;
        g_snprintf(himd->statusmsg,sizeof(himd->statusmsg),
                   "Can't allocate %d bytes for raw string (string idx %d)",
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

char* himd_get_string_utf8(struct himd * himd, unsigned int idx, int*type)
{
    int length;
    char * out;
    char * srcencoding;
    char * rawstr;
    GError * err = NULL;
    g_return_val_if_fail(himd != NULL, NULL);
    g_return_val_if_fail(idx >= 1, NULL);
    g_return_val_if_fail(idx < 4096, NULL);
    
    rawstr = himd_get_string_raw(himd, idx, type, &length);
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
            himd->status = HIMD_ERROR_UNKNOWN_ENCODING;
            g_snprintf(himd->statusmsg,sizeof(himd->statusmsg),
                       "string %d has unknown encoding with ID %d",
                       idx, rawstr[0]);
            himd_free(rawstr);
            return NULL;
    }
    out = g_convert(rawstr+1,length-1,"UTF-8",srcencoding,NULL,NULL,&err);
    himd_free(rawstr);
    if(err)
    {
        himd->status = HIMD_ERROR_STRING_ENCODING_ERROR;
        g_snprintf(himd->statusmsg,sizeof(himd->statusmsg),
                   "convert string %d from %s to UTF-8: %s",
                   idx, srcencoding, err->message);
        return NULL;
    }
    return out;
}

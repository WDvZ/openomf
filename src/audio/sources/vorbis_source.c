#ifdef USE_OGGVORBIS

#include <stdio.h>
#include <stdlib.h>
#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>
#include "audio/sources/vorbis_source.h"
#include "utils/log.h"

typedef struct vorbis_source_t {
    OggVorbis_File src_file;
    int current_section;
} vorbis_source;

const char* vorbis_text_error(int id) {
    switch(id) {
        case OV_EREAD: return "OV_EREAD";
        case OV_ENOTVORBIS: return "OV_ENOTVORBIS";
        case OV_EVERSION: return "OV_EVERSION";
        case OV_EBADHEADER: return "OV_EBADHEADER";
        case OV_EFAULT: return "OV_EFAULT";
        case OV_HOLE: return "OV_HOLE";
        case OV_EBADLINK: return "OV_EBADLINK";
        case OV_EINVAL: return "OV_EINVAL";
        default:
            return "unknown error";
    }
}

int vorbis_source_update(audio_source *src, char *buffer, int len) {
    vorbis_source *local = source_get_userdata(src);
    int ret = ov_read(
            &local->src_file, 
            buffer, // Output buffer
            len, // Output buffer length
            0, // endian byte packing
            2, // Word size
            1, // Signedness, signed.
            &local->current_section // number of the current logical bitstream
        );
    if(ret < 0) {
        DEBUG("Vorbis Source: Error %d while streaming: %s.", ret, vorbis_text_error(ret));
        return 0;
    }
    return ret;
}

void vorbis_source_close(audio_source *src) {
    vorbis_source *local = source_get_userdata(src);
    ov_clear(&local->src_file);
    free(local);
}

int vorbis_source_init(audio_source *src, const char* file) {
    vorbis_source *local;
    int ret;

    // Init local struct
    local = malloc(sizeof(vorbis_source));

    // Try to open up the audio file
    ret = ov_fopen(file, &local->src_file);
    if(ret != 0) {
        PERROR("Vorbis Source: File '%s' could not be opened: ", file, vorbis_text_error(ret));
        goto error_1;
    }

    // Get file information
    vorbis_info *vi = ov_info(&local->src_file, -1);
    char **comment_ptr = ov_comment(&local->src_file, -1)->user_comments;

    // Audio information
    source_set_frequency(src, vi->rate);
    source_set_bytes(src, 2);
    source_set_channels(src, vi->channels);

    // Set callbacks
    source_set_userdata(src, local);
    source_set_update_cb(src, vorbis_source_update);
    source_set_close_cb(src, vorbis_source_close);

    // Some debug info
    DEBUG("Vorbis Source: Loaded file '%s' succesfully (%d Hz, %d ch).", 
        file, vi->rate, vi->channels);
    while(*comment_ptr) {
        DEBUG(" * Comment: %s", *comment_ptr);
        ++comment_ptr;
    }

    // All done
    return 0;
error_1:
    free(local);
    return 1;
}

#endif // USE_OGGVORBIS
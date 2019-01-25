
#include "bmp.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>

struct {
    uint32_t dummy;
    uint16_t val;
} endianness_test = { 0, 0xFF00 };

bool fread_little_value(void* ptr, size_t size, size_t n, FILE* restrict f) {

    uint8_t swap;
    size_t num_read = fread(ptr, size, n, f);
    if (num_read != n)
        return false;
    // possible alignment crash. used uintmax_t to try to guarantee it'd get placed in an acceptable spot
    if (*(unsigned char*)&endianness_test.val) {
        // big endian lands here
        switch (size) {
            case 2:
                swap = *((uint8_t*)ptr + 1);
                *((uint8_t*)ptr + 1) = *(uint8_t*)ptr;
                *(uint8_t*)ptr = swap;
                break;

            case 4:
                // outer
                swap = *((uint8_t*)ptr + 3);
                *((uint8_t*)ptr + 3) = *(uint8_t*)ptr;
                *(uint8_t*)ptr = swap;
                // inner
                swap = *((uint8_t*)ptr + 2);
                *((uint8_t*)ptr + 2) = *((uint8_t*)ptr + 1);
                *((uint8_t*)ptr + 1) = swap;
                break;
            default:
                assert((size == 2) || (size == 4));
                return false;
        }
    }
    return true;
}


/*
// bool x(template<t> buffer, FILE* value_in)
// errno
#define read_deserealize(BUFFER, FP) if (sizeof(*(BUFFER)) != fread((BUFFER), sizeof(*(BUFFER)), 1, (FP))) { return false;}
// bool x(void* buffer, uint cb, uint count, FILE* array_in)
// errno
#define read_deserealize_blob(BUFFER, CB, FP) if ((CB) != fread((BUFFER), 1, (CB), (FP))) { return false; }

#define write_serialize(BUFFER, FP) if (sizeof(*(BUFFER)) != fwrite(BUFFER, sizeof(*(BUFFER)), 1, (FP))) { return false; }

#define write_serialize_blob(BUFFER, CB, FP) if ((CB) != fwrite((BUFFER), 1, (CB), (FP))) { return false; }

// TODO big endianness support (bmps use little endian)

//      DDB FILE:
//
//  //////////////////
//  //  [0x0000]    //
//  //  header      //
//  //              //
//  //////////////////
//  //  system      //
//  //  colormap    //
//  //  index blob  //
//  //////////////////
//
bool parse_bmp_v1(FILE* rfp, UNSERIAL_BITMAPV1* out) {

    errno = 0;
    UNSERIAL_BITMAPV1FILEHEADER k = {0};
    read_deserealize(&k.Type, rfp);
    read_deserealize(&k.Width, rfp);
    read_deserealize(&k.Height, rfp);
    read_deserealize(&k.WidthCb, rfp);
    read_deserealize(&k.Planes, rfp);
    read_deserealize(&k.BitsPerPixel, rfp);

    if (DDB_TYPE_MAGIC != k.Type) {
        fprintf(stderr, "\nCorrupted picture, .DDB->Magic was not equal to 0.");
        errno = EINVAL;
        return false;
    }

    // blob of data in file: ->WidthCb (Width + padding) * ->Height
    // blob = ->Width * ->Height
    unsigned char* colordata;
    if (!(colordata = calloc(1, (k.Width * k.Height) * k.BitsPerPixel)))
        return errno = EADDRINUSE, false;

    // this won't work!
    if (k.WidthCb < k.Width) {
        fprintf(stderr, "\nCorrupted picture, .DDB->ScanlineWidth is less than than image width.");
        errno = EINVAL;
        return false;
    }

    unsigned char* padding;
    if (!(padding = malloc(k.WidthCb - k.Width + 1)))
        return errno = EADDRINUSE, false;

    // read each line without padding
    uint8_t* j = colordata;
    const size_t line_width = k.Width * k.Height,
                 padded_width = k.WidthCb;
    for (uint16_t i = 0; (i < k.Height) && (j < colordata + (k.BitsPerPixel * (k.Width * k.Height))); ++i) {
        if ((k.Width * k.BitsPerPixel) != fread(j, k.BitsPerPixel, k.Width, bmpfp))
            return errno = EBADF, false;
        read_deserealize_blob(j, (k.Width * k.BitsPerPixel), rfp);
        read_deserealize_blob(padding, (k.BitsPerPixel * (k.WidthCb - k.Width)), rfp);
        j += (k.BitsPerPixel * k.Width);
    }

    memcpy(&out->base_hdr, &k, sizeof(k));
    out->blob = colordata;
    errno = 0;
    return true;
}

bool gen_bmp_v1(FILE* wfp, const UNSERIAL_BITMAPV1* in) {

    errno = 0;
    rewind(wfp);

    UNSERIAL_BITMAPV1FILEHEADER hdr = {0};
    memcpy(&hdr, &in->base_hdr, sizeof(hdr));
    // autogenerate some helpful values if they're uninitialized
    // No padding? Seems unlikely but ok
    if (in->base_hdr.WidthCb < in->base_hdr.Width) {
        fprintf(stderr, "\nCorrupted input. .DDB->ScanlineWidth is less than image width.\n\tAssuming 0 padding.");
        hdr.WidthCb = in->base_hdr.Width;
    }
    // multiple planes?? does this look like xcf to you?
    if (in->base_hdr.Planes != 1) {
        fprintf(stderr, "\nCorrupted input. .DDB multiple planes are not supported.\n\tAssuming 1 plane.");
        hdr.Planes = 1;
    }
    
    const uint16_t DDB_FILE = DDB_TYPE_MAGIC;
    write_serialize(&DDB_FILE, wfp);
    write_serialize(&hdr.Width, wfp);
    write_serialize(&hdr.Height, wfp);
    write_serialize(&hdr.WidthCb, wfp);
    write_serialize(&hdr.Planes, wfp);
    write_serialize(&hdr.BitsPerPixel, wfp);

    unsigned char* padding;
    if (!(padding = calloc(hdr.WidthCb - hdr.Width + 1, 1))) 
        return errno = EADDRINUSE, false;

    const unsigned char* j = in->blob;
    for (uint16_t i = 0; (i < hdr.Height) && (j < in->blob + (in->base_hdr.Width * in->base_hdr.Height)); ++i) {
        write_serialize_blob(j, hdr.Width, wfp);
        if (hdr.WidthCb > hdr.Width)
            write_serialize_blob(padding, (hdr.WidthCb - hdr.Width), wfp);
        j += hdr.BitsPerPixel * hdr.Height;
    }

    rewind(wfp);
    errno = 0;
    return true;
}*/

// TODO
bool parse_ddb(FILE* ddb_read, UNSERIAL_BITMAP* out) { return false; }
bool parse_bmp2(FILE* bmp2_read, UNSERIAL_BITMAP* out) {
    return false;
}
bool parse_bmp3(FILE* bmp3_read, UNSERIAL_BITMAP* out);

bool parse_bmp4(FILE* bmp4_read, UNSERIAL_BITMAP* out) { return false; }
bool parse_bmp5(FILE* bmp5_read, UNSERIAL_BITMAP* out) { return false; }

bool parse_bmp_array(FILE* bmp3_array, UNSERIAL_BITMAP* out);

bool parse_bmp(FILE* bmp_read, UNSERIAL_BITMAP* out) {

    UNSERIAL_BITMAP work = {0}, *work_nonlocal;

    // first 16-bit value of bmp file is flag
    // 00 00 for .DDB (v1) and 42 4D "BM" for .BMP (v2+)
    rewind(bmp_read);
    if (!fread_little_value(&work.file.bmp.Type, 2, 1, bmp_read))
        return false;
    
    switch (work.file.bmp.Type) {
        // Bitmap v1 is a different format
        case DDB_TYPE_MAGIC:
        // TODO
            return parse_ddb(bmp_read, out);
        break;
        case BMP_TYPE_MAGIC:
        break;
        default:
            printf("\nInvalid bitmap file: header magic not recognized.");
            return errno = EBADF, false;
    };

    if (!fread_little_value(&work.file.bmp.Size, 4, 1, bmp_read))
        return false;
    if (!fread_little_value(&work.file.bmp.Reserved1, 2, 1, bmp_read))
        return false;
    if (!fread_little_value(&work.file.bmp.Reserved2, 2, 1, bmp_read))
        return false;
    if (!fread_little_value(&work.file.bmp.BlobIndex, 4, 1, bmp_read))
        return false;

    //fseek(bmp_read, CB_SERIALIZED_BITMAPFILEHEADER, SEEK_SET);
    // Info header (version dependent) immediately follows file header
    // Version is determined by size
    if (!fread_little_value(&work.info.bmp3.Size, 4, 1, bmp_read))
        return false;
    // TODO
    // if bmp v2 then width and height are 16bit
    if (!fread_little_value(&work.info.bmp3.Width, ((work.info.bmp2.Size == CB_SERIALIZED_BITMAPV2FILEHEADER) ? 2 : 4), 1, bmp_read))
        return false;
    if (!fread_little_value(&work.info.bmp3.Height, ((work.info.bmp2.Size == CB_SERIALIZED_BITMAPV2FILEHEADER) ? 2 : 4), 1, bmp_read))
        return false;
    if (!fread_little_value(&work.info.bmp3.Planes, 2, 1, bmp_read))
        return false;
    if (!fread_little_value(&work.info.bmp3.BitCount, 2, 1, bmp_read))
        return false;

    // try not to modify out unless on success
    if (!(work_nonlocal = calloc(1, sizeof(UNSERIAL_BITMAP))))
        return errno = EADDRINUSE, false;
    memcpy(work_nonlocal, &work, sizeof(UNSERIAL_BITMAP));
    
    bool sub_bmp_parse_success = false;
    switch (work.info.bmp3.Size) {
        case CB_SERIALIZED_BITMAPV2INFOHEADER:
            sub_bmp_parse_success = parse_bmp2(bmp_read, work_nonlocal);
        break;
        case CB_SERIALIZED_BITMAPV3INFOHEADER:
            printf("v3");
            sub_bmp_parse_success = parse_bmp3(bmp_read, work_nonlocal);
        break;
        case CB_SERIALIZED_BITMAPV4INFOHEADER:
            printf("It's a v4 bitmap\n");
             sub_bmp_parse_success = parse_bmp3(bmp_read, work_nonlocal);
        break;
        case CB_SERIALIZED_BITMAPV5INFOHEADER:
            printf("It's a v5 bitmap");
        break;
        default:
        printf("\nInvalid bitmap file: info header size not recognized.%i", work.info.bmp2.Size);
        errno = EBADF;
    };

    memcpy(out, work_nonlocal, sizeof(UNSERIAL_BITMAP));
    free(work_nonlocal);
    return sub_bmp_parse_success;
}

bool parse_bmp_blob(FILE* bmp_read, UNSERIAL_BITMAP* out) {

}

// assert bmp3_read points to a valid bitmap 3 file parsed by this library
bool parse_bmp3(FILE* bmp3_read, UNSERIAL_BITMAP* out) {

    rewind(bmp3_read);
    //  +4 because width and height are 2 bytes longer each in v3
    fseek(bmp3_read, CB_SERIALIZED_BITMAPFILEHEADER + CB_SERIALIZED_BITMAPV2INFOHEADER + 4, SEEK_SET);
// bmp 3 contains additional fields in the info header
    if (!fread_little_value(&out->info.bmp3.Compression, sizeof(out->info.bmp3.Compression), 1, bmp3_read))
        return false;
    if (!fread_little_value(&out->info.bmp3.SizeImage, sizeof(out->info.bmp3.SizeImage), 1, bmp3_read))
        return false;
    if (!fread_little_value(&out->info.bmp3.XPxPerMeter, sizeof(out->info.bmp3.XPxPerMeter), 1, bmp3_read))
        return false;
    if (!fread_little_value(&out->info.bmp3.YPxPerMeter, sizeof(out->info.bmp3.YPxPerMeter), 1, bmp3_read))
        return false;
    if (!fread_little_value(&out->info.bmp3.ClrUsed, sizeof(out->info.bmp3.ClrUsed), 1, bmp3_read))
        return false;
    if (!fread_little_value(&out->info.bmp3.ClrImportant, sizeof(out->info.bmp3.ClrImportant), 1, bmp3_read))
        return false;

    // special case: windows 3 has extra fields if compression = 3
    if (BMP_COMPRESSION_BITFIELDS == out->info.bmp3.Compression) {
        if (!fread_little_value(&out->info.bmp3_nt.RedMask, 4, 1, bmp3_read))
            return false;
        if (!fread_little_value(&out->info.bmp3_nt.BlueMask, 4, 1, bmp3_read))
            return false;
        if (!fread_little_value(&out->info.bmp3_nt.GreenMask, 4, 1, bmp3_read))
            return false;
    }

    return parse_bmp_array(bmp3_read, out);
}

typedef bool (*bmp_for_each_pixel)(int32_t row, int32_t col, unsigned R, unsigned G, unsigned B);

bool blit_console(const UNSERIAL_BITMAP* in) {
    
    //uint32_t alpha_pixel = *(uint32_t*)in->blob;
    unsigned char* p = in->blob;
    unsigned char alpha_pixel[3] = {0};
    memcpy(alpha_pixel, in->blob, 3);
    for (unsigned int i = 0; i < abs(in->info.bmp3.Height); ++i) {
        printf("\33[%d;%dH", in->info.bmp3.Height - i, 0U);
        for (unsigned int j = 0; j < abs(in->info.bmp3.Width); ++j) {
            if (!memcmp(alpha_pixel, p, 3))
                printf("  ");
            else printf("XX");
            p += 3;
        }
        printf("%i\n", i);
    }
}

bool parse_bmp_compression_none(FILE* bmp_read, UNSERIAL_BITMAP* out) {
    fseek(bmp_read, out->file.bmp.BlobIndex, SEEK_SET);

    fpos_t pixel_array = {0};
    fgetpos(bmp_read, &pixel_array);

    const size_t cbrow = abs(out->info.bmp3.Width) * (out->info.bmp3.BitCount / CHAR_BIT);
    // alpha channel not integrated into bmpv2-
    unsigned char* pix_array = calloc(cbrow * abs(out->info.bmp3.Height), 1);
    if (!pix_array)
        return false;
    
    // padding bytes must be added so that each row in memory is aligned to a DWORD boundary
    size_t pad = (cbrow % 4) ? (4 - (cbrow % 4)) : 0;
    uintmax_t dummy;

    for (size_t i = 0; i < abs(out->info.bmp3.Height); ++i) {
        unsigned char* this_row = pix_array + cbrow * i;
        if (cbrow != fread(this_row, 1, cbrow, bmp_read))
            return false;
        if (pad)
            if (pad != fread(&dummy, pad, 1, bmp_read))
                return false;
    }

    out->blob = pix_array;
    return true;

}

bool parse_bmp_array(FILE* bmp_read, UNSERIAL_BITMAP* out) {
       
    printf("\n");
    fseek(bmp_read, out->file.bmp.BlobIndex, SEEK_SET);

    switch (out->info.bmp3.Compression) {
        case BMP_COMPRESSION_NONE:
            return parse_bmp_compression_none(bmp_read, out);
        case BMP_COMPRESSION_RLE8:
        case BMP_COMPRESSION_RLE4:
        case BMP_COMPRESSION_BITFIELDS:
            printf("Todo.");
            exit(0);
    }
    
    return true;
}

int main() {
    
    FILE* fp = fopen("ie.bmp", "r");
    if (feof(fp)) {
        printf("\nbad file");
        return 0;
    }
    printf("\nfile open.");
    UNSERIAL_BITMAP a ={0};
    parse_bmp(fp, &a);
    blit_console(&a);
    fclose(fp);
    return 0;
}



#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>


// Formats are listed in chronological order due to lots of reverse referencing
//  1: .DDB ( device dependent bitmap ), Windows 1.X
//  2: .BMP ()
// For use with info header Size field to identify bitmap version
// calculated from struct documentation
// V1
#define DDB_TYPE_MAGIC 0U
#define DDB_PLANES_CONST 1
#define CB_SERIALIZED_BITMAPV1FILEHEADER 10

// V2
#define BMP_TYPE_MAGIC 0x4D42U
#define CB_SERIALIZED_BITMAPFILEHEADER  CB_SERIALIZED_BITMAPV2FILEHEADER
#define CB_SERIALIZED_BITMAPV2FILEHEADER 14
#define CB_SERIALIZED_BITMAPV2INFOHEADER 12
// V3
//#define CB_SERIALIZED_BITMAPFILEHEADER 24
#define CB_SERIALIZED_BITMAPV3INFOHEADER 40
// V4
#define CB_SERIALIZED_BITMAPV4INFOHEADER 108
// V5
#define CB_SERIALIZED_BITMAPV5INFOHEADER 124

#define BMP_COMPRESSION_NONE    0
#define BMP_COMPRESSION_RLE8    1
#define BMP_COMPRESSION_RLE4    2
#define BMP_COMPRESSION_BITFIELDS 3 // RGB bitmap with RGB masks (v3 NT)

#define SUSPICIOUS_VALUE(CRITICAL) assert(!CRITICAL)

// Default values for bitmap bitfield masks (3.0 NT compression = BITFIELDS)

// 16bits RGB565
#define BMP_BITFIELD_RGB565_RED(u)    ((0xF8000000UL & u) >> (11 + 16)
#define BMP_BITFIELD_RGB565_GREEN(u)  ((0x07E00000UL & u) >> (5 + 16))
#define BMP_BITFIELD_RGB656_BLUE(u)   ((0x001F0000UL & u) >> (0 + 16))

// 32bits RGB101010
#define BMP_BITFIELD_RGB101010_RED(u)    ((0xFFC00000UL & u) >> 22)
#define BMP_BITFIELD_RGB101010_GREEN(u)  ((0x003FF000UL & u) >> 12)
#define BMP_BITFIELD_RGB101010_BLUE(u)   ((0x00000FFCUL & u) >> 2)

// 32bits RGB888
#define BMP_BITFIELD_RGB888_ALPHA(u)   ((0xFF000000UL & u) >> 24)
#define BMP_BITFIELD_RGB888_RED(u)     ((0x00FF0000UL & u) >> 16)
#define BMP_BITFIELD_RGB888_GREEN(u)   ((0x0000FF00UL & u) >> 8)
#define BMP_BITFIELD_RGB888_BLUE(u)    (0x000000FFUL & u)

// --                   --
// -- deprecated format --
// --                   -- 
// .DDB files contain a header follower by a blob of data. 
//  Probably never going to see this, but it's a public bitmap format
// ->Type should be DDB_TYPE_MAGIC (0)
//      BitsPerPixel is commonly 1, 4, or 8
typedef struct {
	uint16_t Type;        // File type identifier (always 0) 
	uint16_t Width;       // Width of the bitmap in pixels 
	uint16_t Height;      // Height of the bitmap in scan lines 
	uint16_t WidthCb;   // Width of bitmap in bytes (including scan line padding)
	uint8_t  Planes;      // *ignore* Number of color planes (maybe const 1)
	uint8_t  BitsPerPixel;   // Number of bits per pixel (1,4,8)
} UNSERIAL_DDBFILEHEADER;

// --                   --
// -- deprecated format --
// --                   --
//  Image data follows header
//  Parse using blob as index to system color table
// Padding per line = 
typedef struct {
    UNSERIAL_DDBFILEHEADER base_hdr;
    unsigned char*                 blob;
} UNSERIAL_BITMAPV1;

// ===                              ===
// ===  Common  Bitmap Base Header  ===
// ===                              ===
//  In all (Windows 2+) plus bmps
//  ->Type should always be BMP_TYPE_MAGIC ($4D42)
typedef struct 
{
	uint16_t Type;          //File type, always $4D42 ("BM") little endian string
	uint32_t Size;          // Size of the file in bytes (compressed bitmaps only, 0 for uncompressed)
	uint16_t Reserved1;     // Always 0
	uint16_t Reserved2;     // Always 0
	uint32_t BlobIndex;  // Starting position of image data in bytes
} UNSERIAL_BITMAPFILEHEADER;

// --                   --
// -- deprecated format --
// --                   --
typedef struct
{
	uint32_t    Size;            /* Size of this header in bytes */
	int16_t     Width;           /* Image width in pixels */
    int16_t     Padding1;       // added in to aid unions
	int16_t     Height;          /* Image height in pixels */
    int16_t     Padding2;       // added in to aid unions
	uint16_t    Planes;          /* Number of color planes */
	uint16_t    BitCount;    /* Number of bits per pixel */
} UNSERIAL_BITMAPV2INFOHEADER;

// ===                              ===
// ===  Common  Bitmap Base Header  ===
// ===                              ===
//  Most widely used and first publicly documented bmp file format. Before: V3, leaked, and V2, proprietary, also some osx formats.
//  Colloquially known as the info header BITMAPINFOHEADER,
//    others specify Version e.g. BITMAP^V5^INFOHEADER (sans ^^)

// Special case / todo:
//      [ ] Windows CE, Pocket PC Bitmap, 2 bits / pixel
//      [ ] OS/2 may parse identically. 
//      [ ]     Workaround: 
//                  if biCompress is 3 or 4
//                      3: if BitCount is 
//
typedef struct {
    uint32_t Size; // 4 Header size in bytes
    int32_t  Width; // 4 Width of image 
    int32_t  Height; // 4 Height of image
    uint16_t Planes; // 2 Number of colour planes
    uint16_t BitCount; // 2 Bits per pixel
    uint32_t Compression; // 4 Compression type ( 1 = 8bit RLE, 2 = 4bit RLE, 3 = Windows NT )
    uint32_t SizeImage; // 4 /* Image size in uint8_ts
    int32_t  XPxPerMeter; // 4 Horizontal resolution
    int32_t  YPxPerMeter; // 4 Vertical resolution
    uint32_t ClrUsed; // 4 /* Number of colours
    uint32_t ClrImportant; // 4 /* Important colours
} UNSERIAL_BITMAPINFOHEADER; // V3

// When compression = 3, additional struct fields follow
typedef struct {
    uint32_t Size; // 4 Header size in bytes
    int32_t  Width; // 4 Width of image 
    int32_t  Height; // 4 Height of image
    uint16_t Planes; // 2 Number of colour planes
    uint16_t BitCount; // 2 Bits per pixel
    uint32_t Compression; // 4 Compression type ( 1 = 8bit RLE, 2 = 4bit RLE, 3 = Windows NT )
    uint32_t SizeImage; // 4 /* Image size in uint8_ts
    int32_t  XPxPerMeter; // 4 Horizontal resolution
    int32_t  YPxPerMeter; // 4 Vertical resolution
    uint32_t ClrUsed; // 4 /* Number of colours
    uint32_t ClrImportant; // 4 /* Important colours
    uint32_t RedMask;       // Bits of red
    uint32_t GreenMask;     // bits of green
    uint32_t BlueMask;      // bits of blue
} UNSERIAL_BITMAPNTINFOHEADER; // V3

// --                   --
// -- deprecated format --
// --                   -- 
// 
typedef struct {

} DESERIAL_BITMAPV4INFOHEADER;

typedef struct
{   uint32_t Size; // 4 Header size in bytes
    int32_t  Width; // 4 Width of image 
    int32_t  Height; // 4 Height of image
    uint16_t Planes; // 2 Number of colour planes
    uint16_t BitCount; // 2 Bits per pixel
    uint32_t Compression; // 4 Compression type ( 1 = 8bit RLE, 2 = 4bit RLE, 3 = Windows NT )
    uint32_t SizeImage; // 4 /* Image size in uint8_ts
    int32_t  XPxPerMeter; // 4 Horizontal resolution
    int32_t  YPxPerMeter; // 4 Vertical resolution
    uint32_t ClrUsed; // 4 /* Number of colours
    uint32_t ClrImportant; // 4 /* Important colours
    uint32_t RedMask;       // Bits of red
    uint32_t GreenMask;     // bits of green
    uint32_t BlueMask;      // bits of blue
    uint32_t AlphaMask;     /* Mask identifying bits of alpha component */
    uint32_t CSType;        /* Color space type */
    int16_t  RedX;          /* X coordinate of red endpoint */
    int16_t  RedY;          /* Y coordinate of red endpoint */
    int16_t  RedZ;          /* Z coordinate of red endpoint */
    int16_t  GreenX;        /* X coordinate of green endpoint */
    int16_t  GreenY;        /* Y coordinate of green endpoint */
    int16_t  GreenZ;        /* Z coordinate of green endpoint */
    int16_t  BlueX;         /* X coordinate of blue endpoint */
    int16_t  BlueY;         /* Y coordinate of blue endpoint */
    int16_t  BlueZ;         /* Z coordinate of blue endpoint */
    uint32_t GammaRed;      /* Gamma red coordinate scale value */
    uint32_t GammaGreen;    /* Gamma green coordinate scale value */
    uint32_t GammaBlue;     /* Gamma blue coordinate scale value */
} WIN4XBITMAPHEADER;


/*
typedef struct {
  UNSERIAL_BITMAPINFOHEADER old_header;
  uint32_t     RedMask;
  uint32_t     GreenMask;
  uint32_t     BlueMask;
  uint32_t     AphaMask;
  uint32_t     CSType;
  CIEXYZTRIPLE Endpoints;
  uint32_t     GammaRed;
  uint32_t     GammaGreen;
  uint32_t     GammaBlue;
  uint32_t     Intent;
  uint32_t     ProfileData;
  uint32_t     ProfileSize;
  uint32_t     Reserved;
} UNSERIAL_BITMAPV5INFOHEADER;
*/

typedef struct {
    unsigned char    rgbBlue;   
    unsigned char    rgbGreen;
    unsigned char    rgbRed;  
    unsigned char    rgbReserved;
} RGBQUAD;

typedef struct {
    union {
        uint16_t signature;
        UNSERIAL_BITMAPFILEHEADER bmp;
        UNSERIAL_DDBFILEHEADER  ddb;
    } file;
    union {
        uint32_t hdrsize;
        UNSERIAL_BITMAPV2INFOHEADER bmp2;
        UNSERIAL_BITMAPINFOHEADER bmp3;
        UNSERIAL_BITMAPNTINFOHEADER bmp3_nt;
    } info;
    struct {
        size_t count;
        RGBQUAD* array;
    } palette;
    unsigned char* blob;
} UNSERIAL_BITMAP;

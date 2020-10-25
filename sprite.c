/*
 * sprite.c -- part of SpaceRobots2
 * Copyright (C) 2020 Michael Banack <github@banack.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <png.h>

#include "sprite.h"

typedef enum SpriteSource {
    SPRITE_SOURCE_RED,
    SPRITE_SOURCE_BLUE,
    SPRITE_SOURCE_GREEN,
    SPRITE_SOURCE_SHEET1,
    SPRITE_SOURCE_MAX,
    SPRITE_SOURCE_INVALID,
} SpriteSource;

typedef struct SpriteSpec {
    SpriteType type;
    SpriteSource source;
    uint32 x, y, w, h;
} SpriteSpec;

static const SpriteSpec gSpecs[] = {
    { SPRITE_INVALID,          SPRITE_SOURCE_INVALID,  0,   0,   0,   0, },

    { SPRITE_RED_BASE,         SPRITE_SOURCE_RED,      1,   1, 101, 101, },
    { SPRITE_RED_FIGHTER,      SPRITE_SOURCE_RED,    103,   1,  11,  11, },
    { SPRITE_RED_MISSILE,      SPRITE_SOURCE_RED,    115,   1,   7,   7, },
    { SPRITE_RED_POWER_CORE,   SPRITE_SOURCE_RED,    123,   1,   5,   5, },

    { SPRITE_BLUE_BASE,        SPRITE_SOURCE_BLUE,     1,   1, 101, 101, },
    { SPRITE_BLUE_FIGHTER,     SPRITE_SOURCE_BLUE,   103,   1,  11,  11, },
    { SPRITE_BLUE_MISSILE,     SPRITE_SOURCE_BLUE,   115,   1,   7,   7, },
    { SPRITE_BLUE_POWER_CORE,  SPRITE_SOURCE_BLUE,   123,   1,   5,   5, },

    { SPRITE_GREEN_BASE,       SPRITE_SOURCE_GREEN,    1,   1, 101, 101, },
    { SPRITE_GREEN_FIGHTER,    SPRITE_SOURCE_GREEN,  103,   1,  11,  11, },
    { SPRITE_GREEN_MISSILE,    SPRITE_SOURCE_GREEN,  115,   1,   7,   7, },
    { SPRITE_GREEN_POWER_CORE, SPRITE_SOURCE_GREEN,  123,   1,   5,   5, },

    { SPRITE_FIGHTER_BLUE1,    SPRITE_SOURCE_SHEET1, 100,  20,   9,   9, },
    { SPRITE_FIGHTER_BLUE2,    SPRITE_SOURCE_SHEET1, 116,  20,   9,   9, },
    { SPRITE_FIGHTER_BLUE3,    SPRITE_SOURCE_SHEET1, 132,  20,   9,   9, },
    { SPRITE_FIGHTER_BLUE4,    SPRITE_SOURCE_SHEET1, 148,  20,   9,   9, },

    { SPRITE_FIGHTER_GREEN1,   SPRITE_SOURCE_SHEET1, 100,  36,   9,   9, },
    { SPRITE_FIGHTER_GREEN2,   SPRITE_SOURCE_SHEET1, 116,  36,   9,   9, },
    { SPRITE_FIGHTER_GREEN3,   SPRITE_SOURCE_SHEET1, 132,  36,   9,   9, },
    { SPRITE_FIGHTER_GREEN4,   SPRITE_SOURCE_SHEET1, 148,  36,   9,   9, },

    { SPRITE_FIGHTER_RED1,     SPRITE_SOURCE_SHEET1, 100,  52,   9,   9, },
    { SPRITE_FIGHTER_RED2,     SPRITE_SOURCE_SHEET1, 116,  52,   9,   9, },
    { SPRITE_FIGHTER_RED3,     SPRITE_SOURCE_SHEET1, 132,  52,   9,   9, },
    { SPRITE_FIGHTER_RED4,     SPRITE_SOURCE_SHEET1, 148,  52,   9,   9, },

    { SPRITE_MISSILE1,         SPRITE_SOURCE_SHEET1, 102, 110,   6,   6, },
    { SPRITE_MISSILE2,         SPRITE_SOURCE_SHEET1, 118, 110,   6,   6, },
    { SPRITE_MISSILE3,         SPRITE_SOURCE_SHEET1, 134, 110,   6,   6, },

    { SPRITE_CORE1,            SPRITE_SOURCE_SHEET1, 103, 159,   4,   4, },
    { SPRITE_CORE2,            SPRITE_SOURCE_SHEET1, 119, 159,   4,   4, },
};

typedef struct SpriteBacking {
    uint32 refCount;

    bool active;

    SDL_Surface *sdlSurface;

    /*
     * Textures in SDL are tied to a renderer, so the Sprite module
     * creates them on demand as the blit calls come in.
     */
    SDL_Texture *sdlTexture;
    SDL_Renderer *sdlRenderer;
} SpriteBacking;

typedef struct SpriteGlobalData {
    SDL_Surface *sources[SPRITE_SOURCE_MAX];

    uint32 numBacking;
    SpriteBacking backing[1000];
} SpriteGlobalData;

SpriteGlobalData gSprite;

static void SpriteCalcMobSheetSize(uint32 *sheetWidth,
                                   uint32 *sheetHeight);
static SpriteType SpriteGetSpriteType(int playerID, MobType t);
static void SpriteCalcMobSpriteRect(MobType mobType, SDL_Rect *rect);

static SpriteBacking *SpriteGetBacking(uint32 backingID);
static SpriteBacking *SpriteAllocBacking(uint32 *backingID);
static void SpriteFreeBacking(uint32 backingID);
static void SpriteAcquireBacking(uint32 backingID);
static void SpriteReleaseBacking(uint32 backingID);


void Sprite_Init()
{
    ASSERT(MBUtil_IsZero(&gSprite, sizeof(gSprite)));

    ASSERT(ARRAYSIZE(gSprite.sources) == SPRITE_SOURCE_MAX);
    ASSERT(SPRITE_SOURCE_MAX == 4);

    gSprite.sources[SPRITE_SOURCE_RED]   = Sprite_LoadPNG("art/red.png",   129, 103);
    gSprite.sources[SPRITE_SOURCE_BLUE]  = Sprite_LoadPNG("art/blue.png",  129, 103);
    gSprite.sources[SPRITE_SOURCE_GREEN] = Sprite_LoadPNG("art/green.png", 129, 103);
    gSprite.sources[SPRITE_SOURCE_SHEET1] = Sprite_LoadPNG("art/sheet1.png", 200, 200);

    for (int x = 0; x < ARRAYSIZE(gSprite.sources); x++) {
        uint32 backingID;
        SpriteBacking *backing;

        VERIFY(gSprite.sources[x] != NULL);

        backing = SpriteAllocBacking(&backingID);
        ASSERT(backingID == x);
        backing->sdlSurface = gSprite.sources[x];
    }
}

void Sprite_Exit()
{
    for (int x = 0; x < ARRAYSIZE(gSprite.sources); x++) {
        SpriteReleaseBacking(x);

        // The surface was freed by the backing.
        gSprite.sources[x] = NULL;
    }

    for (int x = 0; x < ARRAYSIZE(gSprite.backing); x++) {
        ASSERT(gSprite.backing[x].refCount == 0);
        ASSERT(!gSprite.backing[x].active);
    }
}

static SpriteBacking *SpriteGetBacking(uint32 backingID)
{
    SpriteBacking *backing = &gSprite.backing[backingID];
    ASSERT(gSprite.numBacking < ARRAYSIZE(gSprite.backing));
    ASSERT(backing->active);
    ASSERT(backing->refCount > 0);
    return backing;
}

static SpriteBacking *SpriteAllocBacking(uint32 *pBackingID)
{
    uint32 backingID;
    ASSERT(pBackingID != NULL);
    ASSERT(gSprite.numBacking < ARRAYSIZE(gSprite.backing));

    backingID = gSprite.numBacking;
    gSprite.numBacking++;

    ASSERT(!gSprite.backing[backingID].active);
    gSprite.backing[backingID].active = TRUE;

    ASSERT(gSprite.backing[backingID].refCount == 0);
    SpriteAcquireBacking(backingID);

    *pBackingID = backingID;
    return &gSprite.backing[backingID];
}

static void SpriteFreeBacking(uint32 backingID)
{
    ASSERT(backingID < ARRAYSIZE(gSprite.backing));
    ASSERT(backingID < gSprite.numBacking);
    ASSERT(gSprite.backing[backingID].active);
    ASSERT(gSprite.backing[backingID].refCount == 0);

    if (gSprite.backing[backingID].sdlTexture != NULL) {
        SDL_DestroyTexture(gSprite.backing[backingID].sdlTexture);
    }
    ASSERT(gSprite.backing[backingID].sdlSurface != NULL);
    SDL_FreeSurface(gSprite.backing[backingID].sdlSurface);

    MBUtil_Zero(&gSprite.backing[backingID],
                sizeof(gSprite.backing[backingID]));
}

static void SpriteAcquireBacking(uint32 backingID)
{
    ASSERT(backingID < ARRAYSIZE(gSprite.backing));
    ASSERT(backingID < gSprite.numBacking);
    ASSERT(gSprite.backing[backingID].active);
    gSprite.backing[backingID].refCount++;
}

static void SpriteReleaseBacking(uint32 backingID)
{
    ASSERT(backingID < ARRAYSIZE(gSprite.backing));
    ASSERT(backingID < gSprite.numBacking);
    ASSERT(gSprite.backing[backingID].active);
    ASSERT(gSprite.backing[backingID].refCount > 0);
    gSprite.backing[backingID].refCount--;

    if (gSprite.backing[backingID].refCount == 0) {
        SpriteFreeBacking(backingID);
    }
}

Sprite *Sprite_CreateCircle(uint32 radius, uint32 bgraColor)
{
    Sprite *sprite;
    SpriteBacking *backing;
    uint32 d = 2 * radius + 1;
    SDL_Point cPoint;

    sprite = MBUtil_ZAlloc(sizeof(*sprite));

    backing = SpriteAllocBacking(&sprite->backingID);

    sprite->srcx = 0;
    sprite->srcy = 0;
    sprite->w = d;
    sprite->h = d;

    ASSERT(sprite != NULL);
    backing->sdlSurface = SDL_CreateRGBSurfaceWithFormat(0, d, d, 32,
                                                         SDL_PIXELFORMAT_BGRA32);
    cPoint.x = d / 2;
    cPoint.y = d / 2;
    Sprite_DrawCircle(backing->sdlSurface, bgraColor, &cPoint, radius);

    ASSERT(backing->sdlRenderer == NULL);
    ASSERT(backing->sdlTexture == NULL);

    return sprite;
}

void Sprite_DrawCircle(SDL_Surface *sdlSurface, uint32 color,
                       const SDL_Point *center, int radius)
{
    uint8 *pixels;

    int minX = MAX(0, center->x - radius);
    int maxX = MIN(sdlSurface->w, center->x + radius + 1);
    int minY = MAX(0, center->y - radius);
    int maxY = MIN(sdlSurface->h, center->y + radius + 1);

    SDL_LockSurface(sdlSurface);
    pixels = (uint8 *)sdlSurface->pixels;
    pixels += sdlSurface->pitch * minY;

    for (int y = minY; y < maxY; y++) {
        uint32 *row = (uint32 *)pixels;
        int dy = abs(y - center->y);
        for (int x = minX; x < maxX; x++) {
            int dx = abs(x - center->x);
            if (dx * dx + dy * dy <= radius * radius) {
                row[x] = color;
            }
        }
        pixels += sdlSurface->pitch;
    }

    SDL_UnlockSurface(sdlSurface);
}

Sprite *Sprite_CreateType(SpriteType t)
{
    Sprite *sprite;
    SpriteSource source;

    ASSERT(t < SPRITE_TYPE_MAX);
    ASSERT(t < ARRAYSIZE(gSpecs));
    ASSERT(gSpecs[t].type == t);

    sprite = MBUtil_ZAlloc(sizeof(*sprite));

    source = gSpecs[t].source;
    SpriteAcquireBacking(source);
    sprite->backingID = source;
    ASSERT(source < ARRAYSIZE(gSprite.sources));

    sprite->srcx = gSpecs[t].x;
    sprite->srcy = gSpecs[t].y;
    sprite->w = gSpecs[t].w;
    sprite->h = gSpecs[t].h;

    return sprite;
}


Sprite *Sprite_CreateMob(int playerID, MobType t)
{
    return Sprite_CreateType(SpriteGetSpriteType(playerID, t));
}

static SpriteType SpriteGetSpriteType(int playerID, MobType t)
{
    if (playerID == 0) {
        switch (t) {
            case MOB_TYPE_BASE:
                return SPRITE_RED_BASE;
            case MOB_TYPE_FIGHTER:
                return SPRITE_RED_FIGHTER;
            case MOB_TYPE_MISSILE:
                return SPRITE_RED_MISSILE;
            case MOB_TYPE_POWER_CORE:
                return SPRITE_RED_POWER_CORE;
            default:
                NOT_IMPLEMENTED();
        }
    } else if (playerID == 1) {
        switch (t) {
            case MOB_TYPE_BASE:
                return SPRITE_BLUE_BASE;
            case MOB_TYPE_FIGHTER:
                //return SPRITE_BLUE_FIGHTER;
                return SPRITE_FIGHTER_BLUE1;
            case MOB_TYPE_MISSILE:
                //return SPRITE_BLUE_MISSILE;
                return SPRITE_MISSILE1;
            case MOB_TYPE_POWER_CORE:
                //return SPRITE_BLUE_POWER_CORE;
                return SPRITE_CORE1;
            default:
                NOT_IMPLEMENTED();
        }
    } else if (playerID == 2) {
        switch (t) {
            case MOB_TYPE_BASE:
                return SPRITE_GREEN_BASE;
            case MOB_TYPE_FIGHTER:
                //return SPRITE_GREEN_FIGHTER;
                return SPRITE_FIGHTER_GREEN2;
            case MOB_TYPE_MISSILE:
                //return SPRITE_GREEN_MISSILE;
                return SPRITE_MISSILE2;
            case MOB_TYPE_POWER_CORE:
                //return SPRITE_GREEN_POWER_CORE;
                return SPRITE_CORE2;
            default:
                NOT_IMPLEMENTED();
        }
    } else {
        NOT_IMPLEMENTED();
    }
}

void Sprite_Free(Sprite *s)
{
    ASSERT(s != NULL);
    SpriteReleaseBacking(s->backingID);
    free(s);
}


void Sprite_Blit(Sprite *sprite, SDL_Renderer *r, uint32 x, uint32 y)
{
    SDL_Rect srcRect;
    SDL_Rect destRect;
    SpriteBacking *backing = SpriteGetBacking(sprite->backingID);

    Sprite_PrepareTexture(sprite, r);

    srcRect.x = sprite->srcx;
    srcRect.y = sprite->srcy;
    srcRect.w = sprite->w;
    srcRect.h = sprite->h;

    destRect.x = x;
    destRect.y = y;
    destRect.w = sprite->w;
    destRect.h = sprite->h;

    SDL_RenderCopy(r, backing->sdlTexture, &srcRect, &destRect);
}

void Sprite_BlitCentered(Sprite *s, SDL_Renderer *r, uint32 x, uint32 y)
{
    NOT_IMPLEMENTED();
}

SDL_Surface *Sprite_LoadPNG(const char *fileName,
                            uint32 expectedWidth,
                            uint32 expectedHeight)
{
    SDL_Surface *sdlSurface;

    uint32 pngWidth, pngHeight;
    int colorType;
    int bitDepth;
    int interlaceType;
    int compressionMethod;
    int filterMethod;
    int numPasses;

    FILE *fp = fopen(fileName, "rb");
    VERIFY(fp != NULL);

    png_struct *pngPtr;
    pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    VERIFY(pngPtr != NULL);

    png_info *infoPtr;
    infoPtr = png_create_info_struct(pngPtr);
    VERIFY(infoPtr != NULL);

    if (setjmp(png_jmpbuf(pngPtr))) {
        PANIC("Error reading PNG file\n");
    }

    png_init_io(pngPtr, fp);

    png_read_info(pngPtr, infoPtr);

    png_get_IHDR(pngPtr, infoPtr, &pngWidth, &pngHeight,
                 &bitDepth, &colorType, &interlaceType,
                 &compressionMethod, &filterMethod);

    numPasses = png_set_interlace_handling(pngPtr);
    png_set_bgr(pngPtr);

    png_read_update_info(pngPtr, infoPtr);

    /*
     * XXX: Handle more PNG cases?
     */
    VERIFY(bitDepth == 8);
    VERIFY(colorType == PNG_COLOR_TYPE_RGBA);
    VERIFY(expectedWidth == 0 || pngWidth == expectedWidth);
    VERIFY(expectedHeight == 0 || pngHeight == expectedHeight);
    VERIFY(png_get_rowbytes(pngPtr, infoPtr) == sizeof(uint32) * pngWidth);

    sdlSurface = SDL_CreateRGBSurfaceWithFormat(0, pngWidth, pngHeight, 32,
                                                SDL_PIXELFORMAT_BGRA32);

    SDL_LockSurface(sdlSurface);
    for (int i = 0; i < numPasses; i++) {
        uint8 *pixels = sdlSurface->pixels;
        for (int y = 0; y < pngHeight; y++) {
            png_read_row(pngPtr, pixels, NULL);
            pixels += sdlSurface->pitch;
        }
    }
    SDL_UnlockSurface(sdlSurface);

    png_destroy_read_struct(&pngPtr, &infoPtr, NULL);

    fclose(fp);

    return sdlSurface;
}

void Sprite_SavePNG(const char *fileName, SDL_Surface *sdlSurface)
{
    FILE *fp = fopen(fileName, "wb");
    VERIFY(fp != NULL);

    png_struct *pngPtr;

    pngPtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    VERIFY(pngPtr != NULL);

    png_info *infoPtr;
    infoPtr = png_create_info_struct(pngPtr);
    VERIFY(infoPtr != NULL);

    if (setjmp(png_jmpbuf(pngPtr))) {
        PANIC("Error writing PNG file\n");
    }

    png_init_io(pngPtr, fp);

    uint32 bitDepth = 8;
    png_set_IHDR(pngPtr, infoPtr, sdlSurface->w, sdlSurface->h,
                 bitDepth, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_set_bgr(pngPtr);

    png_write_info(pngPtr, infoPtr);

    SDL_LockSurface(sdlSurface);
    uint8 *pixels = (uint8 *)sdlSurface->pixels;

    for (int y = 0; y < sdlSurface->h; y++) {
        png_write_row(pngPtr, pixels);
        pixels += sdlSurface->pitch;
    }

    png_write_end(pngPtr, NULL);
    SDL_UnlockSurface(sdlSurface);

    png_destroy_write_struct(&pngPtr, &infoPtr);

    fclose(fp);
}


static void SpriteCalcMobSheetSize(uint32 *sheetWidth,
                                   uint32 *sheetHeight)
{
    uint32 dw = 0;
    uint32 dh = 0;

    /*
     * Calculate dimensions of sprite-sheet.
     */
    for (MobType t = MOB_TYPE_MIN; t < MOB_TYPE_MAX; t++) {
        uint32 radius = (uint32)MobType_GetRadius(t);
        uint32 d = 2 * radius + 2;
        dw += d;
        dh = MAX(dh, d);
    }
    dw++;
    dh++;

    if (sheetWidth != NULL) {
        *sheetWidth = dw;
    }
    if (sheetHeight != NULL) {
        *sheetHeight = dh;
    }
}

static void SpriteCalcMobSpriteRect(MobType mobType,
                                    SDL_Rect *rect)
{
    ASSERT(rect != NULL);
    ASSERT(mobType >= MOB_TYPE_MIN);
    ASSERT(mobType < MOB_TYPE_MAX);

    uint d = 0;
    for (MobType t = MOB_TYPE_MIN; t < MOB_TYPE_MAX; t++) {
        SDL_Point cPoint;
        uint32 radius = (uint32)MobType_GetRadius(t);

        d++;

        cPoint.x = d;
        cPoint.y = 1;

        if (mobType == t) {
            rect->x = cPoint.x;
            rect->y = cPoint.y;
            rect->w = 2 * radius + 1;
            rect->h = 2 * radius + 1;
            return;
        }

        d += 1 + (2 * radius);
    }
}


SDL_Surface *Sprite_CreateMobSheet(uint32 bgraColor)
{
    SDL_Surface *spriteSheet;
    uint32 dw;
    uint32 dh;
    uint32 transparentBlack = 0x00000000;

    SpriteCalcMobSheetSize(&dw, &dh);

    spriteSheet = SDL_CreateRGBSurfaceWithFormat(0, dw, dh, 32,
                                                 SDL_PIXELFORMAT_BGRA32);
    VERIFY(spriteSheet != NULL);

    SDL_FillRect(spriteSheet, NULL, transparentBlack);

    /*
     * Draw the circles into the sprite-sheet.
     */
    for (MobType t = MOB_TYPE_MIN; t < MOB_TYPE_MAX; t++) {
        SDL_Point cPoint;
        SDL_Rect rect;
        uint32 radius = (uint32)MobType_GetRadius(t);
        SpriteCalcMobSpriteRect(t, &rect);

        ASSERT(2 * radius + 1 == rect.w);
        ASSERT(2 * radius + 1 == rect.h);

        cPoint.x = rect.x + radius;
        cPoint.y = rect.y + radius;

        Sprite_DrawCircle(spriteSheet, bgraColor, &cPoint, radius);
    }

    return spriteSheet;
}


Sprite *Sprite_CreateFromMobSheet(MobType t,
                                  SDL_Surface *mobSheet)
{
    Sprite *sprite;
    SDL_Rect rect;
    SpriteBacking *backing;
    SDL_Surface *sdlSurface;

    ASSERT(mobSheet != NULL);

    sprite = MBUtil_ZAlloc(sizeof(*sprite));
    backing = SpriteAllocBacking(&sprite->backingID);

    SpriteCalcMobSpriteRect(t, &rect);

    // XXX: We make a new surface instead of sharing the mobSheet.
    sprite->srcx = 0;
    sprite->srcy = 0;
    sprite->w = rect.w;
    sprite->h = rect.h;

    sdlSurface = SDL_CreateRGBSurfaceWithFormat(0, rect.w, rect.h, 32,
                                                SDL_PIXELFORMAT_BGRA32);
    VERIFY(sdlSurface != NULL);
    backing->sdlSurface = sdlSurface;

    ASSERT(rect.w == sdlSurface->w);
    ASSERT(rect.h == sdlSurface->h);
    SDL_BlitSurface(mobSheet, &rect, sdlSurface, NULL);
    return sprite;
}

void Sprite_PrepareTexture(Sprite *sprite, SDL_Renderer *r)
{
    ASSERT(sprite != NULL);

    SpriteBacking *backing = SpriteGetBacking(sprite->backingID);
    ASSERT(backing->sdlRenderer == NULL || backing->sdlRenderer == r);

    if (backing->sdlRenderer != r) {
        /*
         * XXX: Minimize the texture to reflect the sprite size, and
         * not the surface?
         */
        ASSERT(backing->sdlTexture == NULL);
        backing->sdlTexture = SDL_CreateTextureFromSurface(r, backing->sdlSurface);
        backing->sdlRenderer = r;
    }
}

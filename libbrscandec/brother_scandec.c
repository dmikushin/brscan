#include "brother_scandec.h"
#include "brother_bugchk.h"
#include "brother_chreso.h"

#include <string.h>

static HANDLE DAT_00208f00;
static HANDLE DAT_00208f08;
static HANDLE DAT_00208f10;
static byte* DAT_00208f18;
static long malloc_size;
void* bugchk;
void* DAT_00208ef0;

static byte* FUN_001063f3(SCANDEC_WRITE *param_1,size_t *param_2);
static byte* FUN_00106751(SCANDEC_WRITE *param_1,size_t *param_2);
byte* (*DAT_00208f20)(SCANDEC_WRITE *param_1,size_t *param_2);

BOOL ScanDecOpen(SCANDEC_OPEN *param_1)
{
  /*
   * Compute buffer size from dwInLinePixCnt and color type.
   * Verified against disassembly: reads offset 0x18 (dwInLinePixCnt),
   * NOT nOutDataKind as Ghidra incorrectly decompiled.
   */
  if ((param_1->nColorType >> 8 & 1) != 0) {
    /* 8-bit single channel */
    malloc_size = param_1->dwInLinePixCnt;
  }
  else if ((param_1->nColorType >> 9 & 1) != 0) {
    /* 8-bit single channel (gray) */
    malloc_size = param_1->dwInLinePixCnt;
  }
  else {
    /* 3 bytes per pixel (RGB) */
    malloc_size = param_1->dwInLinePixCnt * 3;
  }

  bugchk = bugchk_malloc(malloc_size, __FILE__, __LINE__);
  if (bugchk == 0x0) {
    return 0;
  }

  /* Copy input struct to local, call ChangeResoInit, copy outputs back.
   * ChangeResoInit fills dwOutLinePixCnt, dwOutLineByte, dwOutWriteMaxSize. */
  SCANDEC_OPEN localCopy = *param_1;
  ulong uVar1 = ChangeResoInit((undefined8 *)&localCopy);

  if ((int)uVar1 == 0) {
    bugchk_free(bugchk, __FILE__, __LINE__);
    bugchk = (undefined8 *)0x0;
    return 0;
  }

  param_1->dwOutLinePixCnt  = localCopy.dwOutLinePixCnt;
  param_1->dwOutLineByte    = localCopy.dwOutLineByte;
  param_1->dwOutWriteMaxSize = localCopy.dwOutWriteMaxSize;

  DAT_00208f00 = 0;
  DAT_00208f08 = 0;
  DAT_00208f10 = 0;
  DAT_00208f18 = 0;
  DAT_00208f20 = FUN_001063f3;
  return 1;
}

BOOL ScanDecClose(void)
{ 
  if (bugchk != 0) {
    bugchk_free(bugchk, __FILE__, __LINE__);
    bugchk = 0;
  }
  ChangeResoClose();
  return 1;
}

DWORD ScanDecPageEnd(SCANDEC_WRITE *param_1, INT *param_2)
{
  /*
   * Build a ChangeReso struct on the stack with zeroed data fields
   * (PageEnd flushes remaining buffered lines, no new input).
   * Verified against disassembly at 0x6309-0x63aa.
   */
  struct {
    int     field_00;       /* +0x00: from param_1->nInDataKind */
    int     _pad;           /* +0x04 */
    void   *dataPtr;        /* +0x08: NULL for PageEnd */
    unsigned long dataSize; /* +0x10: 0 for PageEnd */
    void   *writeBuff;      /* +0x18: param_1->pWriteBuff */
    unsigned long writeSize;/* +0x20: param_1->dwWriteBuffSize */
    int     reverWrite;     /* +0x28: param_1->bReverWrite */
  } resoIn;

  resoIn.field_00  = param_1->nInDataKind;
  resoIn._pad      = 0;
  resoIn.dataPtr   = 0;
  resoIn.dataSize  = 0;
  resoIn.writeBuff = param_1->pWriteBuff;
  resoIn.writeSize = param_1->dwWriteBuffSize;
  resoIn.reverWrite = param_1->bReverWrite;

  *param_2 = 0;
  long lVar1 = ChangeResoWriteEnd((undefined8)(uintptr_t)&resoIn, (undefined4 *)param_2);

  DAT_00208ef0 = 0;
  if (DAT_00208f08 != 0) {
    DAT_00208f08 = 0;
  }
  if (DAT_00208f18 != 0) {
    DAT_00208f18 = 0;
  }
  return lVar1;
}

BOOL ScanDecPageStart(void)
{
  DAT_00208ef0 = bugchk;
  if (!bugchk) return 0;

  if ((DAT_00208f00 == 0) || (DAT_00208f08 = DAT_00208f00, DAT_00208f00 != 0)) {
    DAT_00208f20 = FUN_001063f3;
    if (DAT_00208f10 != 0) {
      DAT_00208f18 = DAT_00208f10;
      if ((DAT_00208f10 == 0) && (DAT_00208ef0 = 0, DAT_00208f08 != 0)) {
        DAT_00208f08 = 0;
      }
      DAT_00208f20 = FUN_00106751;
    }
    if (ChangeResoWriteStart() == 0) {
      DAT_00208ef0 = 0;
      if (DAT_00208f08 != 0) {
        DAT_00208f08 = 0;
      }
      if (DAT_00208f18 != 0) {
        DAT_00208f18 = 0;
      }
      return 0;
    }
    return 1;
  }

  DAT_00208ef0 = 0;
  return 0;
}

void ScanDecSetTblHandle(HANDLE param_1, HANDLE param_2)
{
  DAT_00208f00 = param_1;
  DAT_00208f10 = param_2;
  return;
}

DWORD ScanDecWrite(SCANDEC_WRITE *param_1, INT *param_2)
{
  /*
   * Call decode function pointer, then pass result to ChangeResoWrite.
   * Verified against disassembly at 0x6287-0x6308.
   * Key fix: the local struct starts at rbp-0x50, NOT rbp-0x58 as Ghidra claimed.
   */
  size_t decodedSize;
  byte *decodedData = (*DAT_00208f20)(param_1, &decodedSize);

  struct {
    int     field_00;       /* +0x00: param_1->nInDataKind */
    int     _pad;           /* +0x04 */
    void   *dataPtr;        /* +0x08: decoded data pointer */
    unsigned long dataSize; /* +0x10: decoded data size */
    void   *writeBuff;      /* +0x18: param_1->pWriteBuff */
    unsigned long writeSize;/* +0x20: param_1->dwWriteBuffSize */
    int     reverWrite;     /* +0x28: param_1->bReverWrite */
  } resoIn;

  resoIn.field_00   = param_1->nInDataKind;
  resoIn._pad       = 0;
  resoIn.dataPtr    = decodedData;
  resoIn.dataSize   = decodedSize;
  resoIn.writeBuff  = param_1->pWriteBuff;
  resoIn.writeSize  = param_1->dwWriteBuffSize;
  resoIn.reverWrite = param_1->bReverWrite;

  *param_2 = 0;
  return ChangeResoWrite((undefined8)(uintptr_t)&resoIn, (undefined4 *)param_2);
}

byte * FUN_001063f3(SCANDEC_WRITE *param_1,size_t *param_2)
{
  byte bVar1;
  size_t sVar2;
  byte *pbVar3;
  byte *pbVar4;
  size_t local_48;
  size_t local_40;
  char local_32;
  byte *local_30;
  byte *local_28;
  CHAR *local_20;

  local_20 = param_1->pLineData;
  local_40 = param_1->dwLineDataSize;
  *param_2 = 0;
  pbVar4 = DAT_00208ef0;
  if (param_1->nInDataComp == 1) {
    memset(DAT_00208ef0,0,malloc_size);
    local_28 = DAT_00208ef0;
    *param_2 = malloc_size;
  }
  else {
    if (param_1->nInDataComp == 3) {
      local_30 = DAT_00208ef0;
      local_28 = DAT_00208ef0;
      local_48 = malloc_size;
      if (DAT_00208f08 == 0) {
        do {
          if (local_48 == 0) {
            return pbVar4;
          }
          sVar2 = local_40 - 1;
          if (sVar2 == 0) {
            return pbVar4;
          }
          bVar1 = *local_20;
          pbVar3 = local_20 + 1;
          if ((char)bVar1 < '\0') {
            if (bVar1 != 0x80) {
              local_32 = '\x01' - bVar1;
              bVar1 = *pbVar3;
              while ((local_32 != '\0' && (local_48 != 0))) {
                *local_30 = bVar1;
                local_30 = local_30 + 1;
                local_32 = local_32 + -1;
                local_48 = local_48 - 1;
                *param_2 = *param_2 + 1;
              }
              sVar2 = local_40 - 2;
              pbVar3 = local_20 + 2;
            }
          }
          else {
            local_32 = bVar1 + 1;
            local_40 = sVar2;
            local_20 = pbVar3;
            while (((sVar2 = local_40, pbVar3 = local_20, local_32 != '\0' && (local_48 != 0)) &&
                   (local_40 != 0))) {
              *local_30 = *local_20;
              local_30 = local_30 + 1;
              local_32 = local_32 + -1;
              local_48 = local_48 - 1;
              local_40 = local_40 - 1;
              *param_2 = *param_2 + 1;
              local_20 = local_20 + 1;
            }
          }
          local_20 = pbVar3;
          local_40 = sVar2;
        } while (local_40 != 0);
      }
      else {
        do {
          if (local_48 == 0) {
            return pbVar4;
          }
          sVar2 = local_40 - 1;
          if (sVar2 == 0) {
            return pbVar4;
          }
          bVar1 = *local_20;
          pbVar3 = local_20 + 1;
          if ((char)bVar1 < '\0') {
            if (bVar1 != 0x80) {
              local_32 = '\x01' - bVar1;
              bVar1 = *pbVar3;
              while ((local_32 != '\0' && (local_48 != 0))) {
                *local_30 = *(byte *)((ulong)bVar1 + DAT_00208f08);
                local_30 = local_30 + 1;
                local_32 = local_32 + -1;
                local_48 = local_48 - 1;
                *param_2 = *param_2 + 1;
              }
              sVar2 = local_40 - 2;
              pbVar3 = local_20 + 2;
            }
          }
          else {
            local_32 = bVar1 + 1;
            local_40 = sVar2;
            local_20 = pbVar3;
            while (((sVar2 = local_40, pbVar3 = local_20, local_32 != '\0' && (local_48 != 0)) &&
                   (local_40 != 0))) {
              bVar1 = *local_20;
              local_20 = local_20 + 1;
              *local_30 = *(byte *)((ulong)bVar1 + DAT_00208f08);
              local_30 = local_30 + 1;
              local_32 = local_32 + -1;
              local_48 = local_48 - 1;
              local_40 = local_40 - 1;
              *param_2 = *param_2 + 1;
            }
          }
          local_20 = pbVar3;
          local_40 = sVar2;
        } while (local_40 != 0);
      }
    }
    else {
      if (DAT_00208f08 == 0) {
        *param_2 = local_40;
        local_28 = local_20;
      }
      else {
        local_30 = DAT_00208ef0;
        local_28 = DAT_00208ef0;
        local_48 = malloc_size;
        while ((local_48 != 0 && (local_40 != 0))) {
          bVar1 = *local_20;
          local_20 = local_20 + 1;
          *local_30 = *(byte *)((ulong)bVar1 + DAT_00208f08);
          local_30 = local_30 + 1;
          local_48 = local_48 - 1;
          local_40 = local_40 - 1;
          *param_2 = *param_2 + 1;
        }
      }
    }
  }
  return local_28;
}

undefined * FUN_00106751(SCANDEC_WRITE *param_1,size_t *param_2)
{
  byte bVar1;
  byte bVar2;
  long lVar3;
  byte *pbVar4;
  undefined *puVar5;
  byte local_62;
  byte *local_60;
  byte *local_58;
  size_t local_50;
  DWORD local_48;
  char local_3a;
  undefined *local_38;
  undefined *local_30;
  CHAR *local_28;

  local_28 = param_1->pLineData;
  local_48 = param_1->dwLineDataSize;
  local_58 = DAT_00208f18;
  local_60 = DAT_00208f18 + local_48;
  *param_2 = 0;
  puVar5 = DAT_00208ef0;
  if (param_1->nInDataComp == 1) {
    memset(DAT_00208ef0,0,malloc_size);
    local_30 = DAT_00208ef0;
    *param_2 = malloc_size;
  }
  else {
    if (param_1->nInDataComp == 3) {
      local_38 = DAT_00208ef0;
      local_30 = DAT_00208ef0;
      local_50 = malloc_size;
      if (DAT_00208f08 == 0) {
        do {
          if (local_50 == 0) {
            return puVar5;
          }
          lVar3 = local_48 + -1;
          if (lVar3 == 0) {
            return puVar5;
          }
          bVar2 = *local_28;
          pbVar4 = local_28 + 1;
          if ((char)bVar2 < '\0') {
            if (bVar2 != 0x80) {
              local_3a = '\x01' - bVar2;
              bVar2 = *pbVar4;
              while ((local_3a != '\0' && (local_50 != 0))) {
                local_62 = *local_60;
                local_60 = local_60 + 1;
                if (local_62 < bVar2) {
                  local_62 = bVar2 - local_62;
                }
                bVar1 = *local_58;
                local_58 = local_58 + 1;
                if (local_62 < bVar1) {
                  *local_38 = (char)((int)((uint)local_62 * 0x100 - (uint)local_62) /
                                    (int)(uint)bVar1);
                }
                else {
                  *local_38 = 0xff;
                }
                local_38 = local_38 + 1;
                local_3a = local_3a + -1;
                local_50 = local_50 - 1;
                *param_2 = *param_2 + 1;
              }
              lVar3 = local_48 + -2;
              pbVar4 = local_28 + 2;
            }
          }
          else {
            local_3a = bVar2 + 1;
            local_48 = lVar3;
            local_28 = pbVar4;
            while (((lVar3 = local_48, pbVar4 = local_28, local_3a != '\0' && (local_50 != 0)) &&
                   (local_48 != 0))) {
              bVar2 = *local_28;
              local_28 = local_28 + 1;
              local_62 = *local_60;
              local_60 = local_60 + 1;
              if (local_62 < bVar2) {
                local_62 = bVar2 - local_62;
              }
              bVar2 = *local_58;
              local_58 = local_58 + 1;
              if (local_62 < bVar2) {
                *local_38 = (char)((int)((uint)local_62 * 0x100 - (uint)local_62) / (int)(uint)bVar2
                                  );
              }
              else {
                *local_38 = 0xff;
              }
              local_38 = local_38 + 1;
              local_3a = local_3a + -1;
              local_50 = local_50 - 1;
              local_48 = local_48 + -1;
              *param_2 = *param_2 + 1;
            }
          }
          local_28 = pbVar4;
          local_48 = lVar3;
        } while (local_48 != 0);
      }
      else {
        do {
          if (local_50 == 0) {
            return puVar5;
          }
          lVar3 = local_48 + -1;
          if (lVar3 == 0) {
            return puVar5;
          }
          bVar2 = *local_28;
          pbVar4 = local_28 + 1;
          if ((char)bVar2 < '\0') {
            if (bVar2 != 0x80) {
              local_3a = '\x01' - bVar2;
              bVar2 = *pbVar4;
              while ((local_3a != '\0' && (local_50 != 0))) {
                local_62 = *local_60;
                local_60 = local_60 + 1;
                if (local_62 < bVar2) {
                  local_62 = bVar2 - local_62;
                }
                bVar1 = *local_58;
                local_58 = local_58 + 1;
                if (local_62 < bVar1) {
                  *local_38 = *(undefined *)
                               ((int)((uint)local_62 * 0x100 - (uint)local_62) / (int)(uint)bVar1 +
                               DAT_00208f08);
                }
                else {
                  *local_38 = *(undefined *)(DAT_00208f08 + 0xff);
                }
                local_38 = local_38 + 1;
                local_3a = local_3a + -1;
                local_50 = local_50 - 1;
                *param_2 = *param_2 + 1;
              }
              lVar3 = local_48 + -2;
              pbVar4 = local_28 + 2;
            }
          }
          else {
            local_3a = bVar2 + 1;
            local_48 = lVar3;
            local_28 = pbVar4;
            while (((lVar3 = local_48, pbVar4 = local_28, local_3a != '\0' && (local_50 != 0)) &&
                   (local_48 != 0))) {
              bVar2 = *local_28;
              local_28 = local_28 + 1;
              local_62 = *local_60;
              local_60 = local_60 + 1;
              if (local_62 < bVar2) {
                local_62 = bVar2 - local_62;
              }
              bVar2 = *local_58;
              local_58 = local_58 + 1;
              if (local_62 < bVar2) {
                *local_38 = *(undefined *)
                             ((int)((uint)local_62 * 0x100 - (uint)local_62) / (int)(uint)bVar2 +
                             DAT_00208f08);
              }
              else {
                *local_38 = *(undefined *)(DAT_00208f08 + 0xff);
              }
              local_38 = local_38 + 1;
              local_3a = local_3a + -1;
              local_50 = local_50 - 1;
              local_48 = local_48 + -1;
              *param_2 = *param_2 + 1;
            }
          }
          local_28 = pbVar4;
          local_48 = lVar3;
        } while (local_48 != 0);
      }
    }
    else {
      if (DAT_00208f08 == 0) {
        local_38 = DAT_00208ef0;
        local_30 = DAT_00208ef0;
        local_50 = malloc_size;
        while ((local_50 != 0 && (local_48 != 0))) {
          bVar2 = *local_28;
          local_28 = local_28 + 1;
          local_62 = *local_60;
          local_60 = local_60 + 1;
          if (local_62 < bVar2) {
            local_62 = bVar2 - local_62;
          }
          bVar2 = *local_58;
          local_58 = local_58 + 1;
          if (local_62 < bVar2) {
            *local_38 = (char)((int)((uint)local_62 * 0x100 - (uint)local_62) / (int)(uint)bVar2);
          }
          else {
            *local_38 = 0xff;
          }
          local_38 = local_38 + 1;
          local_50 = local_50 - 1;
          local_48 = local_48 + -1;
          *param_2 = *param_2 + 1;
        }
      }
      else {
        local_38 = DAT_00208ef0;
        local_30 = DAT_00208ef0;
        local_50 = malloc_size;
        while ((local_50 != 0 && (local_48 != 0))) {
          bVar2 = *local_28;
          local_28 = local_28 + 1;
          local_62 = *local_60;
          local_60 = local_60 + 1;
          if (local_62 < bVar2) {
            local_62 = bVar2 - local_62;
          }
          bVar2 = *local_58;
          local_58 = local_58 + 1;
          if (local_62 < bVar2) {
            *local_38 = *(undefined *)
                         ((int)((uint)local_62 * 0x100 - (uint)local_62) / (int)(uint)bVar2 +
                         DAT_00208f08);
          }
          else {
            *local_38 = *(undefined *)(DAT_00208f08 + 0xff);
          }
          local_38 = local_38 + 1;
          local_50 = local_50 - 1;
          local_48 = local_48 + -1;
          *param_2 = *param_2 + 1;
        }
      }
    }
  }
  return local_30;
}


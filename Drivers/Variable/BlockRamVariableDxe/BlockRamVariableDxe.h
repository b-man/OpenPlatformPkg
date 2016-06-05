/** @file
  This files implements prototypes for the Firmware Volume Block protocol and
  associated components.

  Copyright (c) 2016, Brian McKenzie. All rights reserved.
  Copyright (c) 2015, Linaro Ltd. All rights reserved.
  Copyright (c) 2015, Hisilicon Ltd. All rights reserved.
  Copyright (c) 2006, Intel Corporation. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _BLOCK_RAM_VARIABLE_DXE_H
#define _BLOCK_RAM_VARIABLE_DXE_H

#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>
#include <Protocol/FirmwareVolumeBlock.h>

#define BLOCK_VARIABLE_SIGNATURE                       SIGNATURE_32('b', 'l', 'k', '0')

typedef struct {
  VENDOR_DEVICE_PATH                  Vendor;
  EFI_DEVICE_PATH_PROTOCOL            End;
} BLOCK_DEVICE_PATH;

struct _BLOCK_VARIABLE_INSTANCE {
  UINT32                              Signature;
  EFI_HANDLE                          Handle;

  BOOLEAN                             Initialized;

  UINTN                               Size;
  EFI_LBA                             StartLba;

  EFI_BLOCK_IO_MEDIA                  Media;
  EFI_BLOCK_IO_PROTOCOL               *BlockIoProtocol;
  EFI_DISK_IO_PROTOCOL                DiskIoProtocol;
  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL FvbProtocol;
  EFI_DEVICE_PATH_PROTOCOL            DevicePath;

  VOID*                               ShadowBuffer;
  UINTN                               ShadowBufferBlockCount;
};

typedef struct _BLOCK_VARIABLE_INSTANCE                BLOCK_VARIABLE_INSTANCE;

//
// Protocol Prototypes
//

EFI_STATUS
EFIAPI
FvbProtocolGetAttributes (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  OUT       EFI_FVB_ATTRIBUTES_2                *Attributes
  )
;

EFI_STATUS
EFIAPI
FvbProtocolSetAttributes (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  IN OUT    EFI_FVB_ATTRIBUTES_2                *Attributes
  )
;

EFI_STATUS
EFIAPI
FvbProtocolGetPhysicalAddress (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  OUT       EFI_PHYSICAL_ADDRESS                *Address
  )
;

EFI_STATUS
EFIAPI
FvbProtocolGetBlockSize (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  IN        EFI_LBA                             Lba,
  OUT       UINTN                               *BlockSize,
  OUT       UINTN                               *NumberOfBlocks
  )
;

EFI_STATUS
EFIAPI
FvbProtocolRead (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  IN        EFI_LBA                             Lba,
  IN        UINTN                               Offset,
  IN OUT    UINTN                               *NumBytes,
  IN OUT    UINT8                               *Buffer
  )
;

EFI_STATUS
EFIAPI
FvbProtocolWrite (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  IN        EFI_LBA                             Lba,
  IN        UINTN                               Offset,
  IN OUT    UINTN                               *NumBytes,
  IN        UINT8                               *Buffer
  )
;

EFI_STATUS
EFIAPI
FvbProtocolEraseBlocks (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  ...
  )
;

VOID
EFIAPI
FvbProtocolVirtualNotifyEvent (
  IN EFI_EVENT                    Event,
  IN VOID                         *Context
  )
;

//
// Volatile I/O Prototypes
//

EFI_STATUS
EFIAPI
FvbVolatileRead (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  IN        EFI_LBA                             Lba,
  IN        UINTN                               Offset,
  IN OUT    UINTN                               *NumBytes,
  IN OUT    UINT8                               *Buffer
  )
;

EFI_STATUS
EFIAPI
FvbVolatileWrite (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  IN        EFI_LBA                             Lba,
  IN        UINTN                               Offset,
  IN OUT    UINTN                               *NumBytes,
  IN        UINT8                               *Buffer
  )
;

EFI_STATUS
EFIAPI
FvbVolatileEraseBlocks (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  ...
  )
;

EFI_STATUS
EFIAPI
FvbVolatileGetPhysicalAddress (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  OUT       EFI_PHYSICAL_ADDRESS                *Address
  )
;

EFI_STATUS
FvbValidateHeader (
  IN EFI_FIRMWARE_VOLUME_HEADER  *FwVolHeader
  )
;

EFI_STATUS
FvbInitVolatileStore (
  IN BLOCK_VARIABLE_INSTANCE                    *Instance,
  UINTN                                         StorageSize
  )
;

//
// Non-Volatile I/O Prototypes
//

EFI_STATUS
EFIAPI
FvbNonVolatileRead (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  IN        EFI_LBA                             Lba,
  IN        UINTN                               Offset,
  IN OUT    UINTN                               *NumBytes,
  IN OUT    UINT8                               *Buffer
  )
;

EFI_STATUS
EFIAPI
FvbNonVolatileWrite (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  IN        EFI_LBA                             Lba,
  IN        UINTN                               Offset,
  IN OUT    UINTN                               *NumBytes,
  IN        UINT8                               *Buffer
  )
;

EFI_STATUS
EFIAPI
FvbNonVolatileEraseBlocks (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  ...
  )
;

EFI_STATUS
EFIAPI
FvbNonVolatileGetPhysicalAddress (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  OUT       EFI_PHYSICAL_ADDRESS                *Address
  )
;

EFI_STATUS
FvbInitNonVolatileStore (
  IN BLOCK_VARIABLE_INSTANCE                    *Instance,
  UINTN                                         StorageSize,
  UINT8											*StorageAddress
  )
;

#endif /* _BLOCK_RAM_VARIABLE_DXE_H */

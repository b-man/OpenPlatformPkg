/** @file
  This files implements the Firmware Volume Block protocol for platforms that
  lack a dedicated flash storage device for storing non-volatile variables.

  Copyright (c) 2016, Brian McKenzie. All rights reserved.
  Copyright (c) 2015-2016, Linaro Limited. All rights reserved.
  Copyright (c) 2015-2016, Hisilicon Limited. All rights reserved.
  Copyright (c) 2006, Intel Corporation. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Guid/EventGroup.h>
#include <Guid/VariableFormat.h>
#include <Guid/SystemNvDataGuid.h>

#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeLib.h>

#include "BlockRamVariableDxe.h"

STATIC EFI_EVENT                 mVarsFvbAddrChangeEvent = NULL;

EFI_STATUS
EFIAPI
FvbProtocolGetAttributes (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  OUT       EFI_FVB_ATTRIBUTES_2                *Attributes
  )
{
  EFI_FVB_ATTRIBUTES_2 FvbAttributes;

  FvbAttributes = (EFI_FVB_ATTRIBUTES_2) (
      EFI_FVB2_READ_ENABLED_CAP | // Reads may be enabled
      EFI_FVB2_READ_STATUS      | // Reads are currently enabled
      EFI_FVB2_STICKY_WRITE     | // A block erase is required to flip bits into EFI_FVB2_ERASE_POLARITY
      EFI_FVB2_MEMORY_MAPPED    | // It is memory mapped
      EFI_FVB2_ERASE_POLARITY     // After erasure all bits take this value (i.e. '1')

  );

  FvbAttributes |= EFI_FVB2_WRITE_STATUS      | // Writes are currently enabled
                   EFI_FVB2_WRITE_ENABLED_CAP;  // Writes may be enabled

  *Attributes = FvbAttributes;

  if (!EfiAtRuntime ()) {
    DEBUG ((DEBUG_BLKIO, "FvbProtocolGetAttributes(0x%X)\n", *Attributes));
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FvbProtocolSetAttributes (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  IN OUT    EFI_FVB_ATTRIBUTES_2                *Attributes
  )
{
  if (!EfiAtRuntime ()) {
    DEBUG ((DEBUG_BLKIO, "FvbProtocolSetAttributes(0x%X) is not supported\n", *Attributes));
  }

  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FvbProtocolGetPhysicalAddress (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  OUT       EFI_PHYSICAL_ADDRESS                *Address
  )
{
  EFI_STATUS Status;

  if (!EfiAtRuntime ()) {
  	Status = FvbNonVolatileGetPhysicalAddress (This, Address);
  } else {
  	Status = FvbVolatileGetPhysicalAddress (This, Address);
  }

  return Status;
}

EFI_STATUS
EFIAPI
FvbProtocolGetBlockSize (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  IN        EFI_LBA                             Lba,
  OUT       UINTN                               *BlockSize,
  OUT       UINTN                               *NumberOfBlocks
  )
{
  BLOCK_VARIABLE_INSTANCE       *Instance;

  Instance = CR (This, BLOCK_VARIABLE_INSTANCE, FvbProtocol, BLOCK_VARIABLE_SIGNATURE);
  *BlockSize = (UINTN) Instance->Media.BlockSize;
  if (!EfiAtRuntime ()) {
  	*NumberOfBlocks = PcdGet32 (PcdNvStorageVariableBlockCount);
  } else {
  	*NumberOfBlocks = Instance->ShadowBufferBlockCount;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FvbProtocolRead (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  IN        EFI_LBA                             Lba,
  IN        UINTN                               Offset,
  IN OUT    UINTN                               *NumBytes,
  IN OUT    UINT8                               *Buffer
  )
{
  EFI_STATUS Status;

  if (!EfiAtRuntime ()) {
    Status = FvbNonVolatileRead (This, Lba, Offset, NumBytes, Buffer);
  } else {
  	Status = FvbVolatileRead (This, Lba, Offset, NumBytes, Buffer);
  }

  return Status;
}

EFI_STATUS
EFIAPI
FvbProtocolWrite (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  IN        EFI_LBA                             Lba,
  IN        UINTN                               Offset,
  IN OUT    UINTN                               *NumBytes,
  IN        UINT8                               *Buffer
  )
{
  EFI_STATUS Status;

  if (!EfiAtRuntime ()) {
    Status = FvbNonVolatileWrite(This, Lba, Offset, NumBytes, Buffer);
  } else {
    Status = FvbVolatileWrite(This, Lba, Offset, NumBytes, Buffer);
  }

  return Status;
}

EFI_STATUS
EFIAPI
FvbProtocolEraseBlocks (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  ...
  )
{
  EFI_STATUS Status;

  Status = EFI_SUCCESS;

  return Status;
}

STATIC BLOCK_VARIABLE_INSTANCE   mBlockVariableInstance = {
  .Signature      = BLOCK_VARIABLE_SIGNATURE,
  .Media          = {
    .MediaId                       = 0,
    .RemovableMedia                = FALSE,
    .MediaPresent                  = TRUE,
    .LogicalPartition              = TRUE,
    .ReadOnly                      = FALSE,
    .WriteCaching                  = FALSE,
    .BlockSize                     = 0,
    .IoAlign                       = 4,
    .LastBlock                     = 0,
    .LowestAlignedLba              = 0,
    .LogicalBlocksPerPhysicalBlock = 0,
  },
  .FvbProtocol    = {
    .GetAttributes        = FvbProtocolGetAttributes,
    .SetAttributes        = FvbProtocolSetAttributes,
    .GetPhysicalAddress   = FvbProtocolGetPhysicalAddress,
    .GetBlockSize         = FvbProtocolGetBlockSize,
    .Read                 = FvbProtocolRead,
    .Write                = FvbProtocolWrite,
    .EraseBlocks          = FvbProtocolEraseBlocks,
  }
};

VOID
EFIAPI
FvbVirtualAddressChangeEvent (
  IN EFI_EVENT                    Event,
  IN VOID                         *Context
  )
{
  EfiConvertPointer (0x0, (VOID **)&(mBlockVariableInstance.ShadowBuffer));
  EfiConvertPointer (0x0, (VOID **)&(mBlockVariableInstance.BlockIoProtocol->Media));

  return;
}


EFI_STATUS
FvbProtocolInitializeDxe (
  IN EFI_HANDLE                   ImageHandle,
  IN EFI_SYSTEM_TABLE             *SystemTable
  )
{
  EFI_LBA                         Lba;
  EFI_HANDLE                      Handle;
  EFI_STATUS                      Status;
  UINT32                          Count;
  UINTN                           NvStorageSize;
  UINT8                           *NvStorageData;

  EFI_DEVICE_PATH_PROTOCOL        *NvBlockDevicePath;
  BLOCK_VARIABLE_INSTANCE         *Instance = &mBlockVariableInstance;

  Instance->Signature = BLOCK_VARIABLE_SIGNATURE;

  DEBUG ((EFI_D_INFO, "Starting Buffered FVB DXE\n"));

  Lba = (EFI_LBA) PcdGet32 (PcdNvStorageVariableBlockLba);
  Count = PcdGet32 (PcdNvStorageVariableBlockCount);
  Instance->Media.BlockSize = PcdGet32 (PcdNvStorageVariableBlockSize);
  NvStorageSize = Count * Instance->Media.BlockSize;
  NvStorageData = (UINT8 *) (UINTN) PcdGet32(PcdFlashNvStorageVariableBase);
  Instance->StartLba = Lba;
  Instance->ShadowBufferBlockCount = Count;

  NvBlockDevicePath = &Instance->DevicePath;
  NvBlockDevicePath = ConvertTextToDevicePath ((CHAR16*)FixedPcdGetPtr (PcdNvStorageVariableBlockDevicePath));

  Status = gBS->LocateDevicePath (&gEfiBlockIoProtocolGuid, &NvBlockDevicePath,
                                  &Instance->Handle);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Warning: Couldn't locate NVM device (status: %r)\n", Status));
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->OpenProtocol (
		  Instance->Handle,
          &gEfiBlockIoProtocolGuid,
		      (VOID **) &Instance->BlockIoProtocol,
          gImageHandle,
          NULL,
          EFI_OPEN_PROTOCOL_GET_PROTOCOL
          );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Warning: Couldn't open NVM device (status: %r)\n", Status));
    return EFI_DEVICE_ERROR;
  }

  WriteBackDataCacheRange (Instance, sizeof(BLOCK_VARIABLE_INSTANCE));

  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
		  &Handle,
		  &gEfiFirmwareVolumeBlockProtocolGuid,
		  &Instance->FvbProtocol,
		  NULL
		  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = FvbInitNonVolatileStore (Instance, NvStorageSize, NvStorageData);
  if (EFI_ERROR (Status)) {
  	return Status;
  }

  Status = FvbInitVolatileStore (Instance, NvStorageSize);
  if (EFI_ERROR (Status)) {
  	return Status;
  }

  Status = gBS->CreateEventEx (
      EVT_NOTIFY_SIGNAL,
      TPL_NOTIFY,
      FvbVirtualAddressChangeEvent,
      NULL,
      &gEfiEventVirtualAddressChangeGuid,
      &mVarsFvbAddrChangeEvent
      );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return Status;
}

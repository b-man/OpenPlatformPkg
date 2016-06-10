/** @file
  This files implements boottime Firmware Volume I/O routines.

  Copyright (c) 2016, Brian McKenzie. All rights reserved.
  Copyright (c) 2015-2016, Linaro Limited. All rights reserved.
  Copyright (c) 2015-2016, Hisilicon Limited. All rights reserved.

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
#include <Library/BlockRamVariableLib.h>

#include "BlockRamVariableDxe.h"


STATIC VOID               *mDataPtr;
EFI_PHYSICAL_ADDRESS      mMapNvStorageVariableBase;

EFI_STATUS
EFIAPI
FvbNonVolatileRead (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  IN        EFI_LBA                             Lba,
  IN        UINTN                               Offset,
  IN OUT    UINTN                               *NumBytes,
  IN OUT    UINT8                               *Buffer
  )
{
  BLOCK_VARIABLE_INSTANCE       *Instance;
  EFI_BLOCK_IO_PROTOCOL         *BlockIo;
  EFI_STATUS                    Status;
  UINTN                         Bytes;

  Instance = CR (This, BLOCK_VARIABLE_INSTANCE, FvbProtocol, BLOCK_VARIABLE_SIGNATURE);
  BlockIo = Instance->BlockIoProtocol;
  Bytes = (Offset + *NumBytes + Instance->Media.BlockSize - 1) / Instance->Media.BlockSize * Instance->Media.BlockSize;

  WriteBackInvalidateDataCacheRange (mDataPtr, Bytes);

  Status = BlockIo->ReadBlocks (BlockIo, BlockIo->Media->MediaId, Instance->StartLba + Lba,
		                Bytes, mDataPtr);

  if (EFI_ERROR (Status)) {
    if (!EfiAtRuntime ()) {
      DEBUG ((EFI_D_ERROR, "%a: StartLba:%x, Lba:%x, Offset:%x, Status:%x\n",
	     __func__, Instance->StartLba, Lba, Offset, Status));
    }
    goto exit;
  }

  CopyMem (Buffer, mDataPtr + Offset, *NumBytes);
  WriteBackDataCacheRange (Buffer, *NumBytes);

exit:
  return Status;
}

EFI_STATUS
EFIAPI
FvbNonVolatileWrite (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  IN        EFI_LBA                             Lba,
  IN        UINTN                               Offset,
  IN OUT    UINTN                               *NumBytes,
  IN        UINT8                               *Buffer
  )
{
  BLOCK_VARIABLE_INSTANCE       *Instance;
  EFI_BLOCK_IO_PROTOCOL         *BlockIo;
  EFI_STATUS                    Status;
  UINTN                         Bytes;

  Instance = CR (This, BLOCK_VARIABLE_INSTANCE, FvbProtocol, BLOCK_VARIABLE_SIGNATURE);
  BlockIo = Instance->BlockIoProtocol;
  Bytes = (Offset + *NumBytes + Instance->Media.BlockSize - 1) / Instance->Media.BlockSize * Instance->Media.BlockSize;
  SetMem (mDataPtr, Bytes, 0x0);
  WriteBackInvalidateDataCacheRange (mDataPtr, Bytes);

  Status = BlockIo->ReadBlocks (BlockIo, BlockIo->Media->MediaId, Instance->StartLba + Lba,
                                Bytes, mDataPtr);

  if (EFI_ERROR (Status)) {
    if (!EfiAtRuntime ()) {
      DEBUG ((EFI_D_ERROR, "%a: failed on reading blocks.\n", __func__));
    }
    goto exit;
  }

  CopyMem (mDataPtr + Offset, Buffer, *NumBytes);
  WriteBackInvalidateDataCacheRange (mDataPtr, Bytes);

  Status = BlockIo->WriteBlocks (BlockIo, BlockIo->Media->MediaId, Instance->StartLba + Lba,
                                 Bytes, mDataPtr);
  if (EFI_ERROR (Status)) {
    if (!EfiAtRuntime ()) {
      DEBUG ((EFI_D_ERROR, "%a: StartLba:%x, Lba:%x, Offset:%x, Status:%x\n",
              __func__, Instance->StartLba, Lba, Offset, Status));
    }
  }

  if (!EfiAtRuntime ()) {
    Status = FvbVolatileWrite(This, Lba, Offset, NumBytes, Buffer);
  }

exit:
  return Status;
}

EFI_STATUS
EFIAPI
FvbNonVolatileEraseBlocks (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  ...
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
FvbValidateHeader (
  IN EFI_FIRMWARE_VOLUME_HEADER  *FwVolHeader
  )
{
  UINT16                      Checksum, TempChecksum;
  VARIABLE_STORE_HEADER       *VariableStoreHeader;
  UINTN                       VariableStoreLength;
  UINTN                       FvLength;

  FvLength = (UINTN) (PcdGet32(PcdFlashNvStorageVariableSize) + PcdGet32(PcdFlashNvStorageFtwWorkingSize) +
      PcdGet32(PcdFlashNvStorageFtwSpareSize));

  //
  // Verify the header revision, header signature, length
  // Length of FvBlock cannot be 2**64-1
  // HeaderLength cannot be an odd number
  //
  if ((FwVolHeader->Revision  != EFI_FVH_REVISION)
      || (FwVolHeader->Signature != EFI_FVH_SIGNATURE)
      || (FwVolHeader->FvLength  != FvLength)
      )
  {
    DEBUG ((EFI_D_ERROR, "%a: No Firmware Volume header present\n", __func__));
    return EFI_NOT_FOUND;
  }

  // Check the Firmware Volume Guid
  if(CompareGuid (&FwVolHeader->FileSystemGuid, &gEfiSystemNvDataFvGuid) == FALSE ) {
    DEBUG ((EFI_D_ERROR, "%a: Firmware Volume Guid non-compatible\n", __func__));
    return EFI_NOT_FOUND;
  }

  // Verify the header checksum
  TempChecksum = FwVolHeader->Checksum;
  FwVolHeader->Checksum = 0;

  Checksum = CalculateSum16((UINT16*)FwVolHeader, FwVolHeader->HeaderLength);

  if (Checksum != TempChecksum) {
    DEBUG ((EFI_D_ERROR, "%a: FV checksum is invalid (Checksum:0x%X)\n", __func__, Checksum));
    return EFI_NOT_FOUND;
  }

  FwVolHeader->Checksum = Checksum;

  VariableStoreHeader = (VARIABLE_STORE_HEADER*)((UINTN)FwVolHeader + FwVolHeader->HeaderLength);

  // Check the Variable Store Guid
  if( CompareGuid (&VariableStoreHeader->Signature, &gEfiVariableGuid) == FALSE ) {
    DEBUG ((EFI_D_ERROR, "%a: Variable Store Guid non-compatible\n", __func__));
    return EFI_NOT_FOUND;
  }

  VariableStoreLength = PcdGet32 (PcdFlashNvStorageVariableSize) - FwVolHeader->HeaderLength;
  if (VariableStoreHeader->Size != VariableStoreLength) {
    DEBUG ((EFI_D_ERROR, "%a: Variable Store Length does not match\n", __func__));
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FvbNonVolatileGetPhysicalAddress (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  OUT       EFI_PHYSICAL_ADDRESS                *Address
  )
{
	*Address = mMapNvStorageVariableBase;

	return EFI_SUCCESS;
}

VOID
EFIAPI
FvbNonVolatileAddressChangeEvent (
  IN EFI_EVENT                        Event,
  IN VOID                             *Context
  )
{
  EfiConvertPointer(0x0, (VOID **)&mDataPtr);
  EfiConvertPointer(0x0, (VOID **)&mMapNvStorageVariableBase);
}

STATIC
EFI_STATUS
FvbInitNonVolatileVariableStore (
  IN BLOCK_VARIABLE_INSTANCE      *Instance,
  IN VOID                         *Headers,
  IN UINTN                        HeadersLength
  )
{
  UINT32                                Count;
  EFI_STATUS                            Status;
  EFI_FIRMWARE_VOLUME_HEADER            *FirmwareVolumeHeader;
  VARIABLE_STORE_HEADER                 *VariableStoreHeader;

  // Check if we already have a valid Fv header
  DEBUG ((EFI_D_INFO, "Validating Fv headers...\n"));
  Status = FvbValidateHeader (Headers);
  if (Status == EFI_SUCCESS) {
  	return Status;
  }

  DEBUG ((EFI_D_ERROR, "%a: Invalid or missing Fv Headers. Attempting recovery.\n", __func__));

  // Erase all data on the block device that is reserved for variable storage
  Count = PcdGet32 (PcdNvStorageVariableBlockCount);
  Status = FvbNonVolatileEraseBlocks (&Instance->FvbProtocol, (EFI_LBA)0, Count, EFI_LBA_LIST_TERMINATOR);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Check if the size of the area is at least one block size
  ASSERT((PcdGet32(PcdFlashNvStorageVariableSize) > 0) && (PcdGet32(PcdFlashNvStorageVariableSize) / Instance->BlockIoProtocol->Media->BlockSize > 0));
  ASSERT((PcdGet32(PcdFlashNvStorageFtwWorkingSize) > 0) && (PcdGet32(PcdFlashNvStorageFtwWorkingSize) / Instance->BlockIoProtocol->Media->BlockSize > 0));
  ASSERT((PcdGet32(PcdFlashNvStorageFtwSpareSize) > 0) && (PcdGet32(PcdFlashNvStorageFtwSpareSize) / Instance->BlockIoProtocol->Media->BlockSize > 0));

  //
  // EFI_FIRMWARE_VOLUME_HEADER
  //
  FirmwareVolumeHeader = (EFI_FIRMWARE_VOLUME_HEADER *)Headers;
  CopyGuid (&FirmwareVolumeHeader->FileSystemGuid, &gEfiSystemNvDataFvGuid);
  FirmwareVolumeHeader->FvLength =
      PcdGet32(PcdFlashNvStorageVariableSize) +
      PcdGet32(PcdFlashNvStorageFtwWorkingSize) +
      PcdGet32(PcdFlashNvStorageFtwSpareSize);

  FirmwareVolumeHeader->Signature = EFI_FVH_SIGNATURE;
  FirmwareVolumeHeader->Attributes = (EFI_FVB_ATTRIBUTES_2) (
                                            EFI_FVB2_READ_ENABLED_CAP   | // Reads may be enabled
                                            EFI_FVB2_READ_STATUS        | // Reads are currently enabled
                                            EFI_FVB2_STICKY_WRITE       | // A block erase is required to flip bits into EFI_FVB2_ERASE_POLARITY
                                            EFI_FVB2_MEMORY_MAPPED      | // It is memory mapped
                                            EFI_FVB2_ERASE_POLARITY     | // After erasure all bits take this value (i.e. '1')
                                            EFI_FVB2_WRITE_STATUS       | // Writes are currently enabled
                                            EFI_FVB2_WRITE_ENABLED_CAP    // Writes may be enabled
                                        );

  FirmwareVolumeHeader->HeaderLength          = sizeof(EFI_FIRMWARE_VOLUME_HEADER) + sizeof(EFI_FV_BLOCK_MAP_ENTRY);
  FirmwareVolumeHeader->Revision              = EFI_FVH_REVISION;
  FirmwareVolumeHeader->BlockMap[0].NumBlocks = PcdGet32 (PcdNvStorageVariableBlockCount);
  FirmwareVolumeHeader->BlockMap[0].Length    = Instance->BlockIoProtocol->Media->BlockSize;

  // BlockMap Terminator
  FirmwareVolumeHeader->BlockMap[1].NumBlocks = 0;
  FirmwareVolumeHeader->BlockMap[1].Length    = 0;
  FirmwareVolumeHeader->Checksum = 0;
  FirmwareVolumeHeader->Checksum = CalculateSum16 ((UINT16*)FirmwareVolumeHeader, FirmwareVolumeHeader->HeaderLength);

  //
  // VARIABLE_STORE_HEADER
  //
  VariableStoreHeader = (VARIABLE_STORE_HEADER*)((UINTN)FirmwareVolumeHeader + FirmwareVolumeHeader->HeaderLength);
  CopyGuid (&VariableStoreHeader->Signature, &gEfiVariableGuid);
  VariableStoreHeader->Size = PcdGet32(PcdFlashNvStorageVariableSize) - FirmwareVolumeHeader->HeaderLength;
  VariableStoreHeader->Format            = VARIABLE_STORE_FORMATTED;
  VariableStoreHeader->State             = VARIABLE_STORE_HEALTHY;

  Status = FvbNonVolatileWrite (&Instance->FvbProtocol, 0, 0, &HeadersLength, Headers);

  return Status;
}

EFI_STATUS
FvbInitNonVolatileStore (
  IN BLOCK_VARIABLE_INSTANCE                    *Instance,
  UINTN                                         StorageSize,
  UINT8											*StorageAddress
  )
{
  EFI_STATUS                      Status;
  VOID                            *Headers;
  UINTN                           HeadersLength;

  mMapNvStorageVariableBase = PcdGet32(PcdFlashNvStorageVariableBase);

  HeadersLength = sizeof(EFI_FIRMWARE_VOLUME_HEADER) + sizeof(EFI_FV_BLOCK_MAP_ENTRY) + sizeof(VARIABLE_STORE_HEADER);

  if (StorageSize < HeadersLength) {
    return EFI_BAD_BUFFER_SIZE;
  }

  Status = gBS->AllocatePages (
              AllocateAddress,
              EfiRuntimeServicesCode,
              EFI_SIZE_TO_PAGES (StorageSize),
              &mMapNvStorageVariableBase
              );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to allocate memory for mirrored NvStorageVariableBase (status: %r)\n", __func__, Status));
    return EFI_INVALID_PARAMETER;
  }

  mDataPtr = AllocateRuntimeZeroPool (StorageSize + Instance->Media.BlockSize);
  if (mDataPtr == NULL) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to allocate buffer.\n", __func__));
    return EFI_BUFFER_TOO_SMALL;
  }
  mDataPtr = ALIGN_POINTER (mDataPtr, Instance->Media.BlockSize);

  Headers = AllocateZeroPool(HeadersLength);
  if (Headers == NULL) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to allocate space for initializing Fv headers.\n", __func__));
    return EFI_OUT_OF_RESOURCES;
  }

  Status = FvbNonVolatileRead (&Instance->FvbProtocol, 0, 0, &HeadersLength, Headers);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = FvbInitNonVolatileVariableStore (Instance, Headers, HeadersLength);
  if (EFI_ERROR (Status)) {
  	DEBUG ((EFI_D_ERROR, "%a: Failed to initialize Fv headers.\n", __func__));
    return Status;
  }

  if (StorageSize > ((EFI_FIRMWARE_VOLUME_HEADER*)Headers)->FvLength) {
    StorageSize = ((EFI_FIRMWARE_VOLUME_HEADER*)Headers)->FvLength;
    StorageSize = ((StorageSize + Instance->Media.BlockSize - 1) / Instance->Media.BlockSize) * Instance->Media.BlockSize;
  }

  Status = FvbNonVolatileRead (&Instance->FvbProtocol, 0, 0, &StorageSize, StorageAddress);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

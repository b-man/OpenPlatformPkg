/** @file
  This files implements runtime Firmware Volume I/O routines.

  Copyright (c) 2016, Brian McKenzie. All Rights Reserved.
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
#include <Library/BlockRamVariableLib.h>

#include "BlockRamVariableDxe.h"


EFI_STATUS
EFIAPI
FvbVolatileRead (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  IN        EFI_LBA                             Lba,
  IN        UINTN                               Offset,
  IN OUT    UINTN                               *NumBytes,
  IN OUT    UINT8                               *Buffer
  )
{
  BLOCK_VARIABLE_INSTANCE       *Instance;
  UINT8                         *DataPtr;

  Instance = CR (This, BLOCK_VARIABLE_INSTANCE, FvbProtocol, BLOCK_VARIABLE_SIGNATURE);

  if ((Offset + *NumBytes) > Instance->Media.BlockSize) {
    *NumBytes = Instance->Media.BlockSize - Offset;
  }

  DataPtr =
      (UINT8*) Instance->VolatileStore +
      MultU64x32 (Lba, (UINT32) Instance->Media.BlockSize) +
      Offset;

  if (*NumBytes > 0) {
    CopyMem (Buffer, DataPtr, *NumBytes);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FvbVolatileWrite (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  IN        EFI_LBA                             Lba,
  IN        UINTN                               Offset,
  IN OUT    UINTN                               *NumBytes,
  IN        UINT8                               *Buffer
  )
{
  BLOCK_VARIABLE_INSTANCE       *Instance;
  UINT8                         *DataPtr;

  Instance = CR (This, BLOCK_VARIABLE_INSTANCE, FvbProtocol, BLOCK_VARIABLE_SIGNATURE);

  if ((Offset + *NumBytes) > Instance->Media.BlockSize) {
    *NumBytes = Instance->Media.BlockSize - Offset;
  }

  DataPtr =
      (UINT8*) Instance->VolatileStore +
      MultU64x32 (Lba, (UINT32) Instance->Media.BlockSize) +
      Offset;

  if (*NumBytes > 0) {
    CopyMem (DataPtr, Buffer, *NumBytes);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FvbVolatileEraseBlocks (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  ...
  )
{
	return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FvbVolatileGetPhysicalAddress (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *This,
  OUT       EFI_PHYSICAL_ADDRESS                *Address
  )
{
	BLOCK_VARIABLE_INSTANCE       *Instance;

	Instance = CR (This, BLOCK_VARIABLE_INSTANCE, FvbProtocol, BLOCK_VARIABLE_SIGNATURE);

	*Address = (EFI_PHYSICAL_ADDRESS)(UINTN) Instance->VolatileStore;

	return EFI_SUCCESS;
}

EFI_STATUS
FvbInitVolatileStore (
  IN BLOCK_VARIABLE_INSTANCE                    *Instance,
  UINTN                                         StorageSize
  )
{
  EFI_STATUS                      Status;

  Instance->VolatileStore = AllocateAlignedRuntimePages (
      EFI_SIZE_TO_PAGES (StorageSize),
      SIZE_64KB
      );
  if (Instance->VolatileStore == NULL) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to allocate volatile store.\n", __func__));
    return EFI_BUFFER_TOO_SMALL;
  }

  SetMem (Instance->VolatileStore, StorageSize, 0xff);

  // Copy non-volatile variable data into volatile store
  Status = FvbNonVolatileRead (&Instance->FvbProtocol, 0, 0, &StorageSize, Instance->VolatileStore);
  if (EFI_ERROR (Status)) {
  	DEBUG ((EFI_D_ERROR, "%a: Failed to copy data to volatile store.\n", __func__));
    return Status;
  }

  // Check if everything was copied correctly
  DEBUG ((EFI_D_INFO, "Validating volatile store Fv header...\n"));
  Status = FvbValidateHeader (Instance->VolatileStore);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

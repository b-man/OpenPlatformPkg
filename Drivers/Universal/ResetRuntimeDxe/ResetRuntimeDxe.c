/** @file

  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiDxe.h>

#include <Guid/EventGroup.h>

#include <Protocol/Reset.h>
#include <Protocol/FirmwareVolumeBlock.h>

#include <Library/PcdLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/EfiResetSystemLib.h>
#include <Library/BlockRamVariableLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Library/MemoryAllocationLib.h>

#include "FvbUtil.h"


typedef struct {
  UINTN                                   StorageSize;
  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL      *FvbProtocol;
} FVB_VARIABLE_STORE_INFO;

VOID  *mBufferPtr;
EFI_SYSTEM_TABLE  *mSystemTable;
EFI_EVENT mResetRuntimeVirtAddrChangeEvent;
FVB_VARIABLE_STORE_INFO *mFvbVariableStoreInfo;

VOID
WriteBrvStoreToDisk (
  VOID
  )
{
#if 0
  EFI_STATUS                          Status;
  UINTN                               StorageSize;
  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *FvbProtocol;

  FvbProtocol = mFvbVariableStoreInfo->FvbProtocol;
  StorageSize = mFvbVariableStoreInfo->StorageSize;

  Status = FvbProtocol->Read(FvbProtocol, 0, 0, &StorageSize, mBufferPtr);
  ASSERT_EFI_ERROR (Status);

  BrvSetBootContextActiveState(TRUE);

  Status = FvbProtocol->Write(FvbProtocol, 0, 0, &StorageSize, mBufferPtr);
  ASSERT_EFI_ERROR (Status);
#endif
}

/**
  Resets the entire platform.

  @param  ResetType             The type of reset to perform.
  @param  ResetStatus           The status code for the reset.
  @param  DataSize              The size, in bytes, of WatchdogData.
  @param  ResetData             For a ResetType of EfiResetCold, EfiResetWarm, or
                                EfiResetShutdown the data buffer starts with a Null-terminated
                                Unicode string, optionally followed by additional binary data.

**/
VOID
EFIAPI
ResetSystemViaLib (
  IN EFI_RESET_TYPE   ResetType,
  IN EFI_STATUS       ResetStatus,
  IN UINTN            DataSize,
  IN VOID             *ResetData OPTIONAL
  )
{
  WriteBrvStoreToDisk();

  LibResetSystem (ResetType, ResetStatus, DataSize, ResetData);
  return;
}

VOID
EFIAPI
ResetRuntimeAddressChangeEvent (
  IN EFI_EVENT            Event,
  IN VOID                 *Context
  )
{
  EfiConvertPointer (0x0, (VOID **)&mBufferPtr);
  EfiConvertPointer (0x0, (VOID **)&mFvbVariableStoreInfo);
  EfiConvertPointer (0x0, (VOID **)&mFvbVariableStoreInfo->FvbProtocol);
  EfiConvertPointer (0x0, (VOID **)&mFvbVariableStoreInfo->FvbProtocol->Read);
  EfiConvertPointer (0x0, (VOID **)&mFvbVariableStoreInfo->FvbProtocol->Write);
}

EFI_STATUS
EFIAPI
InitializeReset (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                           Status;
  EFI_HANDLE                           Handle;
  EFI_PHYSICAL_ADDRESS                 FvbVariableBase;
  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  *FvbProtocol;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR      GcdDescriptor;
  UINTN                                NvStorageSize;

  FvbVariableBase = (EFI_PHYSICAL_ADDRESS) PcdGet64 (PcdFlashNvStorageVariableBase64);
  if (FvbVariableBase == 0) {
    FvbVariableBase = (EFI_PHYSICAL_ADDRESS) PcdGet32 (PcdFlashNvStorageVariableBase);
  }

  mFvbVariableStoreInfo = AllocateRuntimeZeroPool (sizeof (FVB_VARIABLE_STORE_INFO));
  if (mFvbVariableStoreInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  FvbProtocol = NULL;
  Status = GetFvbInfoByAddress (FvbVariableBase, NULL, &FvbProtocol);
  ASSERT_EFI_ERROR (Status);

  mFvbVariableStoreInfo->FvbProtocol = FvbProtocol;

  NvStorageSize = PcdGet32 (PcdFlashNvStorageVariableSize);

  mFvbVariableStoreInfo->StorageSize = NvStorageSize;

  mBufferPtr = AllocateAlignedRuntimePages (
                    EFI_SIZE_TO_PAGES (NvStorageSize),
                    SIZE_64KB
                    );
  if (mBufferPtr == NULL) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to allocate pages.\n", __func__));
    return EFI_OUT_OF_RESOURCES;
  }
  SetMem(mBufferPtr, NvStorageSize, 0xff);

  // Ensure that entire flash region is runtime accessable
  Status = gDS->GetMemorySpaceDescriptor (FvbVariableBase, &GcdDescriptor);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: Failed to get firmware volume memory attributes.\n", __func__));
  } else {
    Status = gDS->SetMemorySpaceAttributes (
                    FvbVariableBase,
                    NvStorageSize,
                    GcdDescriptor.Attributes | EFI_MEMORY_RUNTIME
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_WARN, "%a: Driver failed to add EFI_MEMORY_RUNTIME attribute to firmware volume.\n", __func__));
    }
  }

  LibInitializeResetSystem (ImageHandle, SystemTable);

  SystemTable->RuntimeServices->ResetSystem = ResetSystemViaLib;

  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gEfiResetArchProtocolGuid,
                  NULL,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  ResetRuntimeAddressChangeEvent,
                  NULL,
                  &gEfiEventVirtualAddressChangeGuid,
                  &mResetRuntimeVirtAddrChangeEvent
                  );
  ASSERT_EFI_ERROR (Status);

  return Status;
}

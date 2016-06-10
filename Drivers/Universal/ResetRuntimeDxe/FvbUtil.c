/** @file
  This code is derived from MdeModulePkg's Runtime Variable DXE driver.

Copyright (c) 2006 - 2015, Intel Corporation. All rights reserved.<BR>
(C) Copyright 2015 Hewlett Packard Enterprise Development LP<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/FirmwareVolumeBlock.h>


/**
  Retrive the FVB protocol interface by HANDLE.

  @param[in]  FvBlockHandle     The handle of FVB protocol that provides services for
                                reading, writing, and erasing the target block.
  @param[out] FvBlock           The interface of FVB protocol

  @retval EFI_SUCCESS           The interface information for the specified protocol was returned.
  @retval EFI_UNSUPPORTED       The device does not support the FVB protocol.
  @retval EFI_INVALID_PARAMETER FvBlockHandle is not a valid EFI_HANDLE or FvBlock is NULL.

**/
STATIC
EFI_STATUS
GetFvbByHandle (
  IN  EFI_HANDLE                          FvBlockHandle,
  OUT EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  **FvBlock
  )
{
  //
  // To get the FVB protocol interface on the handle
  //
  return gBS->HandleProtocol (
                FvBlockHandle,
                &gEfiFirmwareVolumeBlockProtocolGuid,
                (VOID **) FvBlock
                );
}

/**
  Function returns an array of handles that support the FVB protocol
  in a buffer allocated from pool.

  @param[out]  NumberHandles    The number of handles returned in Buffer.
  @param[out]  Buffer           A pointer to the buffer to return the requested
                                array of  handles that support FVB protocol.

  @retval EFI_SUCCESS           The array of handles was returned in Buffer, and the number of
                                handles in Buffer was returned in NumberHandles.
  @retval EFI_NOT_FOUND         No FVB handle was found.
  @retval EFI_OUT_OF_RESOURCES  There is not enough pool memory to store the matching results.
  @retval EFI_INVALID_PARAMETER NumberHandles is NULL or Buffer is NULL.

**/
STATIC
EFI_STATUS
GetFvbCountAndBuffer (
  OUT UINTN                               *NumberHandles,
  OUT EFI_HANDLE                          **Buffer
  )
{
  EFI_STATUS                              Status;

  //
  // Locate all handles of Fvb protocol
  //
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiFirmwareVolumeBlockProtocolGuid,
                  NULL,
                  NumberHandles,
                  Buffer
                  );
  return Status;
}

/**
  Get the proper fvb handle and/or fvb protocol by the given Flash address.

  @param[in]  Address       The Flash address.
  @param[out] FvbHandle     In output, if it is not NULL, it points to the proper FVB handle.
  @param[out] FvbProtocol   In output, if it is not NULL, it points to the proper FVB protocol.

**/
EFI_STATUS
GetFvbInfoByAddress (
  IN  EFI_PHYSICAL_ADDRESS                Address,
  OUT EFI_HANDLE                          *FvbHandle OPTIONAL,
  OUT EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL  **FvbProtocol OPTIONAL
  )
{
  EFI_STATUS                              Status;
  EFI_HANDLE                              *HandleBuffer;
  UINTN                                   HandleCount;
  UINTN                                   Index;
  EFI_PHYSICAL_ADDRESS                    FvbBaseAddress;
  EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL      *Fvb;
  EFI_FVB_ATTRIBUTES_2                    Attributes;
  UINTN                                   BlockSize;
  UINTN                                   NumberOfBlocks;

  HandleBuffer = NULL;
  //
  // Get all FVB handles.
  //
  Status = GetFvbCountAndBuffer (&HandleCount, &HandleBuffer);
  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  //
  // Get the FVB to access variable store.
  //
  Fvb = NULL;
  for (Index = 0; Index < HandleCount; Index += 1, Status = EFI_NOT_FOUND, Fvb = NULL) {
    Status = GetFvbByHandle (HandleBuffer[Index], &Fvb);
    if (EFI_ERROR (Status)) {
      Status = EFI_NOT_FOUND;
      break;
    }

    //
    // Ensure this FVB protocol supported Write operation.
    //
    Status = Fvb->GetAttributes (Fvb, &Attributes);
    if (EFI_ERROR (Status) || ((Attributes & EFI_FVB2_WRITE_STATUS) == 0)) {
      continue;
    }

    //
    // Compare the address and select the right one.
    //
    Status = Fvb->GetPhysicalAddress (Fvb, &FvbBaseAddress);
    if (EFI_ERROR (Status)) {
      continue;
    }

    //
    // Assume one FVB has one type of BlockSize.
    //
    Status = Fvb->GetBlockSize (Fvb, 0, &BlockSize, &NumberOfBlocks);
    if (EFI_ERROR (Status)) {
      continue;
    }

    if ((Address >= FvbBaseAddress) && (Address < (FvbBaseAddress + BlockSize * NumberOfBlocks))) {
      if (FvbHandle != NULL) {
        *FvbHandle  = HandleBuffer[Index];
      }
      if (FvbProtocol != NULL) {
        *FvbProtocol = Fvb;
      }
      Status = EFI_SUCCESS;
      break;
    }
  }
  FreePool (HandleBuffer);

  if (Fvb == NULL) {
    Status = EFI_NOT_FOUND;
  }

  return Status;
}

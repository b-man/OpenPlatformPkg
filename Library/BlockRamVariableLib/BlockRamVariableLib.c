/** @file
  This files implements shared routines for the BlockRamVariableDxe driver.

  Copyright (c) 2016, Brian McKenzie. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Library/DebugLib.h>
#include <Library/BlockRamVariableLib.h>
#include <Library/MemoryAllocationLib.h>


STATIC BOOLEAN mBrvBootContextActive = FALSE;

VOID
EFIAPI
BrvSetBootContextActiveState (
  IN BOOLEAN      State
  )
{
  mBrvBootContextActive = State;
}

BOOLEAN
EFIAPI
BrvBootContextActive (
  VOID
  )
{
  return mBrvBootContextActive;
}

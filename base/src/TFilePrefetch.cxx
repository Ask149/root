// @(#)root/base:$Name:  $:$Id: TFilePrefetch.cxx,v 1.157 2006/05/04 17:08:54 rdm Exp $
// Author: Rene Brun   18/05/2006

/*************************************************************************
 * Copyright (C) 1995-2000, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include "TFile.h"
#include "TFilePrefetch.h"

ClassImp(TFilePrefetch)

//______________________________________________________________________________
TFilePrefetch::TFilePrefetch() : TObject()
{
   // default Constructor.

   fBufferSize  = 0;
   fBufferLen   = 0;
   fNseek       = 0;
   fNtot        = 0;
   fNb          = 0;
   fSeekSize    = 0;
   fSeek        = 0;
   fSeekIndex   = 0;
   fSeekSort    = 0;
   fPos         = 0;
   fSeekLen     = 0;
   fSeekSortLen = 0;
   fSeekPos     = 0;
   fLen         = 0;
   fFile        = 0;
   fBuffer      = 0;
   fIsSorted    = kFALSE;
}

//_____________________________________________________________________________
TFilePrefetch::TFilePrefetch(TFile *file, Int_t buffersize)
           : TObject()
{
   // creates a TFilePrefetch data structure
   if (buffersize <=10000) fBufferSize = 100000;
   fBufferSize  = buffersize;
   fBufferLen   = 0;
   fNseek       = 0;
   fNtot        = 0;
   fNb          = 0;
   fSeekSize    = 10000;
   fSeek        = new Long64_t[fSeekSize];
   fSeekIndex   = new Long64_t[fSeekSize];
   fSeekSort    = new Long64_t[fSeekSize];
   fPos         = new Long64_t[fSeekSize];
   fSeekLen     = new Int_t[fSeekSize];
   fSeekSortLen = new Int_t[fSeekSize];
   fSeekPos     = new Int_t[fSeekSize];
   fLen         = new Int_t[fSeekSize];
   fFile        = file;
   fBuffer      = new char[fBufferSize];
   fIsSorted    = kFALSE;
   if (file) file->SetFilePrefetch(this);
}

//_____________________________________________________________________________
TFilePrefetch::~TFilePrefetch()
{
   // destructor
   
   delete [] fSeek;
   delete [] fSeekIndex;
   delete [] fSeekSort;
   delete [] fSeekLen;
   delete [] fSeekSortLen;
   delete [] fSeekPos;
   delete [] fBuffer;
}
   

//_____________________________________________________________________________
void TFilePrefetch::Prefetch(Long64_t pos, Int_t len)
{
   // Add block of length len at position pos in the list of blocks to be prefetched
   // if pos <= 0 the current blocks (if any) are reset
   
   fIsSorted = kFALSE;
   if (pos <= 0) {
      fNseek = 0;
      fNtot  = 0;
      return;
   }
   if (fNseek >= fSeekSize) {
      //must reallocate buffers in this place
   }
   fSeek[fNseek] = pos;
   fSeekLen[fNseek] = len;
   fNseek++;
   fNtot += len;
}
   

//_____________________________________________________________________________
void TFilePrefetch::Print(Option_t *option) const
{
   // Print class internal structure
   TString opt = option;
   opt.ToLower();
   printf("Number of blocks: %d, total size : %d\n",fNseek,fNtot);
   if (!opt.Contains("a")) return;
   for (Int_t i=0;i<fNseek;i++) {
      if (fIsSorted && !opt.Contains("s")) {
         printf("block: %5d, from: %lld to %lld, len=%d bytes\n",i,fSeekSort[i],fSeekSort[i]+fSeekSortLen[i],fSeekSortLen[i]);
      } else {
         printf("block: %5d, from: %lld to %lld, len=%d bytes\n",i,fSeek[i],fSeek[i]+fSeekLen[i],fSeekLen[i]);
      }
   }
   printf ("Number of long buffers = %d\n",fNb);
   for (Int_t j=0;j<fNb;j++) {
      printf("fPos[%d]=%lld, fLen=%d\n",j,fPos[j],fLen[j]);
   }
      
}

//_____________________________________________________________________________
Bool_t TFilePrefetch::ReadBuffer(char *buf, Long64_t pos, Int_t len)
{
   // Read buffer at position pos
   // if pos is in the list of prefetched blocks read from fBuffer,
   // otherwise normal read from file
   
   if (fNseek > 0 && !fIsSorted) {
      Sort();
      fFile->ReadBuffers(fBuffer,fPos,fLen,fNb);
      fFile->Seek(pos);
   }
   Int_t loc = (Int_t)TMath::BinarySearch(fNseek,fSeekSort,pos);
   if (loc >= 0 && loc <fNseek && pos == fSeekSort[loc]) {
      memcpy(buf,&fBuffer[fSeekPos[loc]],len);
      //printf("TFilePrefetch::ReadBuffer, pos=%lld, len=%d, slen=%d, loc=%d\n",pos,len,fSeekSortLen[loc],loc);
      return kTRUE;
   } else {
      return kFALSE;
   }
}

//_____________________________________________________________________________
void TFilePrefetch::Sort()
{
   // Sort buffers to be prefetched in increasing order of positions
   // Merge consecutive blocks if necessary
   
   if (!fNseek) return;
   TMath::Sort(fNseek,fSeek,fSeekIndex,kFALSE);
   Int_t i;
   Int_t nb = 0;
   for (i=0;i<fNseek;i++) {
      Long64_t ind = fSeekIndex[i];
      fSeekSort[i] = fSeek[ind];
      fSeekSortLen[i] = fSeekLen[ind];
   }
   if (fNtot > fBufferSize) {
      fBufferSize = fNtot + 100;
      delete [] fBuffer;
      fBuffer = new char[fBufferSize];
     // printf("CHANGING fBufferSize=%d, fNseek=%d, fNtot=%d\n",fBufferSize, fNseek,fNtot);
   }
   fPos[0]  = fSeekSort[0];
   fLen[0]  = fSeekSortLen[0];
   fSeekPos[0] = 0;
   for (i=1;i<fNseek;i++) {
      fSeekPos[i] = fSeekPos[i-1] + fSeekSortLen[i-1];
      if (fSeekSort[i] != fSeekSort[i-1]+fSeekSortLen[i-1]) {
         nb++;
         fPos[nb] = fSeekSort[i];
         fLen[nb] = fSeekSortLen[i];
      } else {
         fLen[nb] += fSeekSortLen[i];
      }
   }
   fNb = nb+1;
   fIsSorted = kTRUE;
}



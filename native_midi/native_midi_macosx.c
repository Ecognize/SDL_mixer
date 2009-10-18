/*
    native_midi_macosx:  Native Midi support on Mac OS X for the SDL_mixer library
    Copyright (C) 2009  Ryan C. Gordon

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Ryan C. Gordon
    icculus@icculus.org
*/

/* This is Mac OS X only, using Core MIDI.
   Mac OS 9 support via QuickTime is in native_midi_mac.c */

#include "SDL_config.h"

#if __MACOSX__

#include <AudioToolbox/AudioToolbox.h>

#include "SDL_endian.h"
#include "native_midi.h"

/* Native Midi song */
struct _NativeMidiSong
{
    MusicPlayer player;
    MusicSequence sequence;
    MusicTimeStamp endTime;
};

static NativeMidiSong *currentsong = NULL;

static OSStatus
GetSequenceLength(MusicSequence sequence, MusicTimeStamp *_sequenceLength)
{
    // http://lists.apple.com/archives/Coreaudio-api/2003/Jul/msg00370.html
    // figure out sequence length
    UInt32 ntracks, i;
    MusicTimeStamp sequenceLength = 0;
    OSStatus err;

    err = MusicSequenceGetTrackCount(sequence, &ntracks);
    if (err != noErr)
        return err;

    for (i = 0; i < ntracks; ++i)
    {
        MusicTrack track;
        MusicTimeStamp tracklen = 0;
        UInt32 tracklenlen = sizeof (tracklen);

        err = MusicSequenceGetIndTrack(sequence, i, &track);
        if (err != noErr)
            return err;

        err = MusicTrackGetProperty(track, kSequenceTrackProperty_TrackLength,
                                    &tracklen, &tracklenlen);
        if (err != noErr)
            return err;

        if (sequenceLength < tracklen)
            sequenceLength = tracklen;
    }

    *_sequenceLength = sequenceLength;

    return noErr;
}


int native_midi_detect()
{
    return 1;  /* always available. */
}

NativeMidiSong *native_midi_loadsong(const char *midifile)
{
    NativeMidiSong *retval = NULL;
    SDL_RWops *rw = SDL_RWFromFile(midifile, "rb");
    if (rw != NULL) {
        retval = native_midi_loadsong_RW(rw);
        SDL_RWclose(rw);
    }

    return retval;
}

NativeMidiSong *native_midi_loadsong_RW(SDL_RWops *rw)
{
    NativeMidiSong *retval = NULL;
    void *buf = NULL;
    int len = 0;
    CFDataRef data = NULL;

    if (SDL_RWseek(rw, 0, RW_SEEK_END) < 0)
        goto fail;
    len = SDL_RWtell(rw);
    if (len < 0)
        goto fail;
    if (SDL_RWseek(rw, 0, RW_SEEK_SET) < 0)
        goto fail;

    buf = malloc(len);
    if (buf == NULL)
        goto fail;

    if (SDL_RWread(rw, buf, len, 1) != 1)
        goto fail;

    retval = malloc(sizeof(NativeMidiSong));
    if (retval == NULL)
        goto fail;

    memset(retval, '\0', sizeof (*retval));

    if (NewMusicPlayer(&retval->player) != noErr)
        goto fail;
    if (NewMusicSequence(&retval->sequence) != noErr)
        goto fail;

    data = CFDataCreate(NULL, (const UInt8 *) buf, len);
    if (data == NULL)
        goto fail;

    free(buf);
    buf = NULL;

    #if 1 /* this is deprecated, but works back to 10.3 */
    if (MusicSequenceLoadSMFDataWithFlags(retval->sequence, data, 0) != noErr)
        goto fail;
    #else  /* not deprecated, but requires 10.5 or later */
    if (MusicSequenceLoadData(retval->sequence, data, 0, 0) != noErr)
        goto fail;
    #endif

    CFRelease(data);
    data = NULL;

    if (GetSequenceLength(retval->sequence, &retval->endTime) != noErr)
        goto fail;

    if (MusicPlayerSetSequence(retval->player, retval->sequence) != noErr)
        goto fail;

    return retval;

fail:
    if (retval) {
        if (retval->sequence)
            DisposeMusicSequence(retval->sequence);
        if (retval->player)
            DisposeMusicPlayer(retval->player);
        free(retval);
    }

    if (data)
        CFRelease(data);

    if (buf)
        free(buf);

    return NULL;
}

void native_midi_freesong(NativeMidiSong *song)
{
    if (song != NULL) {
        if (currentsong == song)
            currentsong = NULL;
        MusicPlayerStop(song->player);
        DisposeMusicSequence(song->sequence);
        DisposeMusicPlayer(song->player);
        free(song);
    }
}

void native_midi_start(NativeMidiSong *song)
{
    if (song == NULL)
        return;

	SDL_PauseAudio(1);
	SDL_UnlockAudio();

    if (currentsong)
        MusicPlayerStop(currentsong->player);

    currentsong = song;
    MusicPlayerStart(song->player);

	SDL_LockAudio();
	SDL_PauseAudio(0);
}

void native_midi_stop()
{
    if (currentsong) {
    	SDL_PauseAudio(1);
	    SDL_UnlockAudio();
        MusicPlayerStop(currentsong->player);
        currentsong = NULL;
    	SDL_LockAudio();
	    SDL_PauseAudio(0);
    }
}

int native_midi_active()
{
    MusicTimeStamp currentTime = 0;
    if (currentsong == NULL)
        return 0;

    MusicPlayerGetTime(currentsong->player, &currentTime);
    return ((currentTime < currentsong->endTime) ||
            (currentTime >= kMusicTimeStamp_EndOfTrack));
}

void native_midi_setvolume(int volume)
{
    /* !!! FIXME: call MusicSequenceGetAUGraph(), figure out where the output
       !!! FIXME:  audio unit is, and change its gain */
}

const char *native_midi_error(void)
{
    return "";  /* !!! FIXME */
}

#endif /* Mac OS X native MIDI support */


#!/bin/bash
# usage: addaudio.sh <inputroot> <outputroot> [--separate-tracks]
#
# By default all speaker wavs are mixed down into one audio track.
# With --separate-tracks every speaker gets its own audio track instead.
inputroot=$1
outputroot=$2
separate=0
case "$3" in
    --separate-tracks|-s) separate=1 ;;
    "") ;;
    *) echo "usage: $0 <inputroot> <outputroot> [--separate-tracks]" >&2; exit 1 ;;
esac

(cd ${outputroot} && ls -1 *.mov > filelist.txt)    # define your list of files (edit text file)
mkdir -p ${outputroot}/MoovAtom                # create a new directory for (silent) annotated movies corrected for playability
mkdir -p ${outputroot}/OverlayAndAudio         # create a new directory for playable sound movies with annotations
while read -r f; do
    [ -z "$f" ] && continue
    rm -f "${outputroot}/MoovAtom/${f}"
    ffmpeg -i "${outputroot}/${f}" -c copy -movflags +faststart "${outputroot}/MoovAtom/${f}"
done < ${outputroot}/filelist.txt


ls -1 ${outputroot}/MoovAtom/ > ${outputroot}/movies.txt;
while read -r f; do
    [ -z "$f" ] && continue
    y=${f:0:8};
    echo "y ${y}";
    ls -1 ${inputroot}/${y}/SeparatedSpeakersAudio/*.wav 2>/dev/null | tr '\n' '\0' | xargs -0 -n 1 basename > ${outputroot}/audios.txt 2>/dev/null;
    args=( -i "${outputroot}/MoovAtom/${f}" );
    filterin="";
    n=0;
    while read -r g; do
        [ -z "$g" ] && continue
        n=$((n+1))
        args+=( -i "${inputroot}/${y}/SeparatedSpeakersAudio/${g}" )
        filterin="${filterin}[${n}:a]"
    done < ${outputroot}/audios.txt
    if [ ${n} -gt 0 ]; then
        if [ ${separate} -eq 1 ]; then
            # one audio track per speaker
            maps=( -map 0:v )
            for i in $(seq 1 ${n}); do maps+=( -map "${i}:a" ); done
        else
            # all speakers mixed down into a single audio track
            args+=( -filter_complex "${filterin}amix=inputs=${n}:duration=longest[aout]" )
            maps=( -map 0:v -map "[aout]" )
        fi
        echo "m ${maps[*]}";
        rm -f "${outputroot}/OverlayAndAudio/${f}";
        args+=( "${maps[@]}" -c:v copy -acodec aac -strict experimental -f mp4 "${outputroot}/OverlayAndAudio/${f}" );
        echo "ffmpeg ${args[*]}";
        ffmpeg "${args[@]}";
    fi;
done < ${outputroot}/movies.txt

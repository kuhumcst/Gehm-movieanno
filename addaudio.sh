#!/bin/bash
inputroot=$1
outputroot=$2
(cd ${outputroot} && ls -1 *.mov > filelist.txt)    # define your list of files (edit text file)
mkdir ${outputroot}/MoovAtom                # create a new directory for (silent) annotated movies corrected for playability
mkdir ${outputroot}/OverlayAndAudio         # create a new directory for playable sound movies with annotations  
for f in `cat ${outputroot}/filelist.txt`; do rm -f "${outputroot}/MoovAtom/${f}"; ffmpeg -i ${outputroot}/${f} -c copy -movflags +faststart ${outputroot}/MoovAtom/${f}; done;


ls -1 ${outputroot}/MoovAtom/ > ${outputroot}/movies.txt; 
for f in `cat ${outputroot}/movies.txt`; do 
    y=${f:0:8}; 
    echo "y ${y}"; 
    ls -1 ${inputroot}/${y}/SeparatedSpeakersAudio/*.wav 2>/dev/null | tr '\n' '\0' | xargs -0 -n 1 basename > ${outputroot}/audios.txt 2>/dev/null; 
    s="ffmpeg -i ${outputroot}/MoovAtom/${f} "; 
    m="-map 0:v "; 
    msav=${m}; 
    #-map 1:a -map 2:a 
    n=0; 
    for g in `cat ${outputroot}/audios.txt`; do n=$((n+1)); s="${s} -i ${inputroot}/${y}/SeparatedSpeakersAudio/${g} "; m="${m} -map ${n}:a ";  done; 
    if [[ "$m" != "$msav" ]]; then 
        echo "m ${m}"; 
        rm -f "${outputroot}/OverlayAndAudio/${f}";
        s="${s} -c:v copy -acodec aac -strict "experimental" -filter_complex amix=inputs=${n}:duration=longest -f mp4  ${outputroot}/OverlayAndAudio/${f}"; 
        echo ${s}; 
        ${s}; 
    fi;
done;


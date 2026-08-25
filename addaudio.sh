cd OpenCVannotatedMovies
ls -1 *.mov > filelist.txt    # define your list of files (edit text file)
mkdir MoovAtom             # create a new directory
for f in `cat filelist.txt`; do  ffmpeg -i ${f} -c copy -movflags +faststart MoovAtom/${f}; done;
cd ..

ls -1 OpenCVannotatedMovies/MoovAtom/ > movies.txt; 
for f in `cat movies.txt`; do 
    y=${f:0:8}; 
    echo "y ${y}"; 
    ls -1 FinalDataset/${y}/SeparatedSpeakersAudio/*.wav 2>/dev/null | tr '\n' '\0' | xargs -0 -n 1 basename > audios.txt 2>/dev/null; 
    s="ffmpeg -i OpenCVannotatedMovies/MoovAtom/${f} "; 
    m="-map 0:v "; 
    msav=${m}; 
    #-map 1:a -map 2:a 
    n=0; 
    for g in `cat audios.txt`; do n=$((n+1)); s="${s} -i FinalDataset/${y}/SeparatedSpeakersAudio/${g} "; m="${m} -map ${n}:a ";  done; 
    if [[ "$m" != "$msav" ]]; then 
        echo "m ${m}"; 
        s="${s} -c:v copy -acodec aac -strict "experimental" -filter_complex amix=inputs=${n}:duration=longest -f mp4  OpenCVannotatedMovies/OverlayAndAudio/${f}"; 
        echo ${s}; 
        ${s}; 
    fi;
done;


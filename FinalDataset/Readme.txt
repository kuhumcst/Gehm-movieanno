BJ 2026.08.25

Copy these data here:

https://cst.dk/download/GEHM/FinalDataset/

Or: Create a folder structure as follows:



<root> / <session1> / OpenPoseKeypoints                 / <session1>.zip 
                      OriginalAudio                     / <session1>.wav
                      OriginalVideo                     / <session1>.mov
                      SeparatedSpeakersAudio            / <session1>-<speaker1>.wav
                                                          <session1>-<speaker2>.wav
                                                          ...
                      SeparatedSpeakersVideo            / <session1>-<speaker1>.mov
                                                          <session1>-<speaker2>.mov
                                                          ...
                      <session1>_Praat_long.TextGrid
         <session2> / (etc.)




The <session1>.zip must have the following structure:

<session1> / <speaker1> / <frame0>.json
                          <frame1>.json
                          ...
             <speaker2> / (etc.)
             ...


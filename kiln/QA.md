# KILN hardware checks

The developer confirmed the 0.2.5 hardware pass. Use these checks when porting the machine to a new SDK or preparing another release. The calibration figures below are historical host measurements.

1. In TE Builder, measure and build KILN with your regular collection. Confirm the usual 4 MiB budget and final size check in your profile. Select KILN on a send.
2. Send a bass and drums to FX I. In Collision, compare Alt K1 at 0%, 50% and 100%, keeping Drive and the track sends fixed. Listen for bass weight, softened hat grit on kick hits, and enough available drive.
3. Hold Shift and turn K1 through all eight modes. Check the names, main-page knobs, alternate page, audio transitions and both stereo channels. In Dynamic Dirt repeat the Warmth check with a bass and a snare.
4. On Stereo Gate use full send, Mix and Depth. Try Hocket, change Rate, stop and restart transport, then set Custom. Set only one side of steps 8 and 16. Compare the audible sides with the two rows on screen.
5. Put Tuned Dirt on FX II at a different root, and leave it ringing while you switch FX I between modes. Check for cross-talk, old tails or stuck sound. Listen to Transient Space on a short pluck.
6. In a test project, put Stereo Gate on FX I and Collision on FX II. Make the left and right Custom gate rows different, including steps 8 and 16. Set distinctive Warmth and Mix values on FX II. Save, open a different project, then reopen the saved one. Before turning any knob, listen to both sends and compare their modes, knob positions and gate rows against the saved setup.
7. In a dense project with both sends in use, listen for crackle. Profile callback mean and peak on hardware separately.

8. Select Spall with Shift + K1 immediately after Tuned Dirt. Play a pad and rimshot on one send. With Fall at zero, move the rimshot across a chord change. Try overlapping hits and maximum Settle. Save with Spall on FX I and a custom gate on FX II, open another project, then reopen the saved one and check both sends.
9. When upgrading a 0.1.1 project, reselect its mode and check gate steps 1–8 on each affected send and pattern before saving.

10. With the same bass/drum loop and track send, compare bypass and Dynamic Dirt at midpoint and maximum Drive. Repeat for Memory Drive, Tuned Dirt and Collision. Verify that cancelling the wet mix restores the source level. Repeat with a quiet pad and with a hot full mix. Try Spall with two active tails on both sends in a busy project and check the hardware block budget.

11. Solo a quiet snare with space between hits. Set full send and Mix, Fracture high and Settle long. Compare minimum and maximum Grain, Fall and Settle on successive hits. Compare Alt K1 Warmth at 0% and 100%. Pull Fracture to zero and let the last tail finish. Then repeat with a quieter track send.
12. Compare Dynamic Dirt against Mix at zero on the same repeating loop. Let it run for two seconds per setting. Add a second track to the send and listen for whether the blend stays balanced. Try Stereo Gate separately to check its open levels and rests.

### Level calibration

For 0.2.1, across 20 loops, three input levels and five Drive settings (300 Dynamic Dirt cases), the median absolute K-weighted difference from dry fell from 1.65 dB to 1.23 dB. The 90th percentile fell from 4.01 dB to 2.82 dB. The Spall onset test detected all eight authored rim hits at each of nine levels from the original audition loop down to −48.2 dB relative to that loop, with no extra hits. The host sound tests also compare rendered tails for all four main controls and Warmth on a single quiet hit.


The host comparison used 20 stereo loops, three input levels, 100% effect Mix and five Drive positions. At the middle input level and midpoint controls, median K-weighted level differences from bypass measured +14.4 dB to 0.0 dB for Dynamic Dirt, +11.0 to +0.2 dB for Memory Drive, and +5.4 to +0.4 dB for Collision. The first value is the original processing and the second is 0.2.0. These are ungated energy measurements over identical windows. Individual sources, input level and track routing affect the result.

### Spall 0.2.2

Use a single low pluck or mellow 110 Hz bass note with an 8 ms attack. Give each note room to decay. Select Spall with full track send, full effect Mix and maximum Settle. Keep the first three main encoders at midpoint, Warmth at zero, and listen for a pitched fragment after release. Compare Fracture at zero with the same input. The host regression checks one capture with an audible tail on each of five separated notes. A second fixture checks high-frequency transients over a pad. An isolated chord onset after a rest now triggers in its own right.

### Tuned Dirt 0.2.2

Play a quiet, repeating C2 pluck. Select C2 Root, maximum Drive and Ring, minimum Damping, and full Mix. Listen for the ringing fifth and compare loudness against Mix at zero after two seconds. The regression plays the same pluck for four seconds and checks that the settled wet level stays within 3 dB of dry. Mode switching and recall use the same saved controls as 0.2.1.

### Landscape 0.2.3

On FX I or FX II, cycle all eight modes. Compare the mode glyph and name. Sweep the four main knobs and Mix, then adjust Warmth in Collision, Dynamic Dirt and Spall. Switch pages and hold Shift. In Stereo Gate, set a custom left-only first step and right-only last step at full Depth, then check the front two rows. Repeat after reopening the project. Both color and grayscale artwork use the same geometry.

### Ridge rider 0.2.4

Select KILN on both sends. Route a spaced snare to FX I only. In Spall, watch a fracture pass left beneath the rider on a strong hit. FX II should stay still. Swap sends and repeat. Let the return fall silent, switch modes and reopen the project. Check the resting rider and saved settings.

### Quiet sends in 0.2.5

Use the same send level at which the rider stood still in 0.2.4. Space the hits about half a second apart. Check that the rider lifts and the fractures travel left, then let the tail die and check that the rider rests.

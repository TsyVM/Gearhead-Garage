# Gearhead Garage: what the original program does

Everything here was read out of the retail files and `ghg.exe` (build of 2002-04-22, ASPack-packed;
unpacked by running it under a debugger to its exit and dumping the image). Addresses are the
unpacked image's (base 0x400000). The game code (`VMech`, compiled unoptimised, 0x40a000-0x46ffff)
sits behind an incremental-link jump table at 0x401005; the libraries (RatDepot, the 3DGE engine,
ALIBI image loading, zlib 1.0.4, libpng, IJG JPEG, the C runtime) are optimised code after it.

## Files

### Resource archives (`Data\*.dat`, `Data\Jobs\random.dat`)

A resource path such as `Gfx24\Fonts\Standard.tga` is looked up by trying each folder prefix as an
archive: `Gfx24.dat` holds `fonts/standard.tga` (names lower case, `/` separated). (0x47e500)

1. A text header (`This is an internal binary data file...`) ending in `0x1A 0x00`.
2. 16 bytes whose two halves XOR to `cLib!317` (0x47e320).
3. A chain of 0x13C-byte directory entries from there to the end of the file (0x47dca0). Each is
   XORed with the key table (below), then its first seven dwords are XORed with the seven dwords at
   +0x120. Layout: `+0 flags, +4 data offset, +8 stored size, +0xC original size, +0x10 next
   entry, +0x14 FILETIME, +0x1C name[260], +0x120 mask[28]`. The first entry is always
   `the lstream header` (skipped).
4. A member (0x47e5c0): read `stored` bytes; if `flags & 1`, decrypt every whole 8-byte block with
   Thin-ICE (Matthew Kwan's ICE, level 0, 8 rounds); then always XOR with the key table from index
   0; then if `flags & 2`, zlib-inflate to `orig` bytes.

Key table (0x47d2a0): 1024 bytes, `k[i] = i & 0xFF`, then 7 rounds over the 138-character string
at 0x4f1708 (`98ru91nb98fH...`), a running index `s` into the string wrapping at its length:

    for round in 0..6: for i in 0..1023:
        k[i] += round + 0x11
        k[i]  = k[i] * str[s]                       (low byte)
        k[(i + 0x13D) & 0x3FF] += str[len-1-s] + k[i]
        s = (s + 1) % len

ICE key (0x47ec50): `7f 2d 1d 3b 44 c7 fe 86`, each byte `i & 7` XORed with the 40 bytes at
0x4f1794 in turn.

| Archive | Members | Contents |
|---|---|---|
| `Gfx24.dat` | 92 | 640x480 screens (base/UP/DN), overlays, pointers, fonts, splash (24-bit TGA) |
| `Gfx8.dat` | 92 | the same for the 8-bit software renderer |
| `Scenes.dat` | 24 | `auction.3ds`, `carlot.3ds`, `jyengine/jybody/jyrgear.3ds` and their textures |
| `Sound16.dat` / `Sound8.dat` | 84 | WAV sounds and music loops |
| `Jobs\random.dat` | 60 | `comments.txt` and customer portraits (`name-h.jpg` happy, `-s.jpg` sad) |

### Tagged files (`.car .dpk .jpk .mek`)

`u32 (type << 28 | id)`, `u32 size`, then `size` bytes. Types: 8 container, 2 int32, 4 float,
6 string (no terminator), 0 raw. Tag ids are `group * 100 + index` in the exe's name table
(0x4ec52c): group 0 `VERSION CARFILE DECALFILE MECHANICFILE JOBFILE UPDATEFILE
EVERBLAZEPLAYERFILE`, 1 `CARDATA DECALDATA MECHANIC JOBDATA JOBPHASEIMAGE ...`, 2 `CD_*`, 3 `PD_*`,
4 `TEX_*`, 5 `MMAT_*`, 6 `MAT_*`, 7 `MESH_*`, 8 `POLY_*`, 9 `VERT_*`, 10 `BITMAP_*`, 11 `DD_*`,
12 `PART_*`, 13 `PBANK_*`, 14 `MECH_*`, 15 `CAR_*`, 16 `DECAL_*`, 17 `JOB_*`, 18 `JP_*`, 19 `JS_*`,
20 `FL_*`, 21 `IS_*`, 22 `UPDATE_*`, 23 `EVERPLAYER_*`, 24 `EVERSCORE_*`. Each name is a global tag
object built by a static initialiser (their addresses are named `T_<NAME>` in the RE notes).

All 165 `.car` files in the install parse with this schema (no unknown tags).

### Cars (`CARFILE > CARDATA`)

- `CD_ID`, `CD_NAME`, `CD_MINSKILL` (0-4), `CD_TEXTURE*`, `CD_MATERIAL*`, `CD_AUTOMESH*`,
  `CD_SPECMESH*`, `CD_PARTDATA*`, optional `CD_SOUND_IGNITION` (`IS_PARTID`, `IS_WAVFILE`).
- Texture: `TEX_NAME` (the artist's path, not loaded), `TEX_FLAGS`, `TEX_BITMAP` (`BITMAP_WIDTH,
  HEIGHT, FORMAT` 16 = RGB565 / 8 = paletted (`BITMAP_PALETTE`) / 32, `BITMAP_DATA`).
- Material: `MAT_NAME, MAT_COLOR` (3 floats), `MAT_BLENDSRC/DST` (1,0 opaque; 4,5 alpha; 1,1
  additive), `MAT_SHADEALPHA`, `MAT_SHADECOLOR` (1 flat, 2 smooth), `MAT_ALPHA`, `MAT_FLAGS`,
  `MAT_TEXINDEX1/2`. Material index 666 in a polygon = the car's paint.
- Mesh: `MESH_NAME` (`#` engine, `$` body, `^` running gear; others are the fixed chassis),
  `MESH_INDEX`, `MESH_LOC`, `MESH_MATRIXX/Y/Z`, `MESH_VERT` (`VERT_LOC`, `VERT_NORMAL`),
  `MESH_POLY` (three `POLY_VERTINDEX`, three `POLY_MAP` UVs, `POLY_MATINDEX`).
- `CD_SPECMESH` `(*)`: a copy of a body mesh drawn again with the additive `SpecMat` (the shine).
- Part (`PartData`, read at 0x43fb8d): `PD_ID` (+4), `PD_NAME` (+0), `PD_CUSTOM` (+0x98, a
  customisation: never on a new car), `PD_REGION` (+0xA4: 1 engine, 2 body, 3 running gear),
  `PD_MESHNAME` (+0xAC), `PD_COSTMIN` (+0x90), `PD_COSTMAX` (+0x8C), `PD_ATTACHDEP` (+0x4C, up to
  16, -1 ends: parts that must be on first), `PD_REMOVEDEP` (+0xC, up to 16: parts that must come
  off first), `PD_MUTEXCSET` (+0xA0, a bit mask: parts sharing a bit replace each other),
  `PD_AMEA` (+0xA8, "attach mutex equivalent allowed": an attach dependency is met by any part
  sharing its mutex bits), `PD_VIC` (+0x99, "visible in Complete": 0 hides the part in the Complete
  view, used for crankshafts and flywheels), `PD_SPECIAL` (+0x9C: 1 the engine block, 2 the starter
  (needed to crank), 3 spins while the engine runs (crank, flywheel, fan), 4 an accessory: always
  perfect, never in the JunkYard), `PD_BOLT*` (float3, car space), `PD_MULTIMAT` (+0xD0: several
  `MMAT_MATERIALINDEX` and one `MMAT_SWITCHPOLYLIST`, the part's own materials as runs: polygon i
  takes material k until i == list[k] - 1; used to put the materials back after a repair).

## Program structure

- Command line (0x4ef050): `-CarID -Skill -NoSound -ShowFPS -JobID -Software -SoundHi -SoundLow
  -MustBeHere -AllowScreenshots -PrintableSnaps`. Without `-MustBeHere` it writes `Missing
  "-MustBeHere" command line parameter.` to `Output.log` and quits (the launcher passes it).
  `-CarID n` starts in the WorkShop as mechanic `CarDev` (id 666, $50,000); `-JobID n` as
  `JobDev` (id 667, $100).
- Main (0x45d18e): creates the JukeBox, video, loading screen, fonts, cars, jobs, then the screens
  (WorkShop 0xC83C bytes, Catalog, Auction, CarLot, JunkYard, SignIn, Credits, JobEngine) and runs
  a 10 ms tick: messages, then `Main_tick` (0x45afba) turns the mouse and keyboard into events
  (0x10001 left held, 0x10002 left pressed, 0x10003 left released, 0x10004 right held, 0x10005
  right pressed, 0x10007 moved, 0x10009 tick, 0x20001 key down, 0x20002 key up, 0x30001 redraw)
  for the active screen. Pointer events (class 1) reach only the widgets under the pointer
  (0x40c2d3), so a widget's "tick" runs while it is hovered.
- Blocking waits: a sound played "to the end" (repair, scrap, the bolts of a cancelled wrench job)
  spins `Main_tick` until it stops; dragging a part (0x45b860) is the same kind of loop.

## Mechanics

Mechanic (0x3B8 bytes): `+4 name, +8 id, +0xC cash (float), +0x10 skill, +0x14 total work time,
+0x34 the last assembly error, +0x80 message box, +0x24C completed-job bit set (64 dwords),
+0x34C..0x358 sounds (rattle12 remove, toilet2 scrap, repair, cashreg), +0x35C decals, +0x374 the
car in the WorkShop, +0x378 total career cars, +0x37C cars, +0x394 the job running, +0x398 auto
job request, +0x39C the WorkShop car's parts bank, +0x3A0 parts banks`.

The file (save 0x45f351, load 0x45ef45), in this order: `MECH_NAME, MECH_ID, MECH_CASH` (float),
`MECH_SKILLINDEX, MECH_NUMCURAUTOS` (the number of cars owned; written, never read),
`MECH_TOTALCAREERAUTOS, MECH_TOTALWORKTIME` (seconds), `MECH_LASTAUTONUM` (the career number of
the car in the WorkShop; left out when there is none or a job is running), the parts banks, the
cars, the decals, `MECH_COMPLETEDJOBS` (64 dwords, raw), `MECH_AUTOJOBREQUEST`. A car
(`MECH_AUTO`, 0x442b5e): `CAR_ID, CAR_ISTOP, CAR_REPAIRTIME` (int, milliseconds),
`CAR_REPAIRCOST` and `CAR_PURCHASECOST` (floats), `CAR_CAREERNUM`, the parts, `CAR_PAINTONTEX`
(always written). A parts bank: `PBANK_AUTOID`, then `PBANK_INBINPART` (the Parts Bin) and
`PBANK_INYARDPART` (the JunkYard shelves) records.

Money (0x46011d): a purchase fails with "You don't have the cash!" (`* Bummer *`) when cash is
short; otherwise cash drops, the WorkShop car's repair cost grows by the amount (unless it is at
Top condition), and `misc\cashreg.wav` plays.

Skills (table 0x4f0578, `TSkill{index, name, six floats}`):

| # | Name | f0 | f1 | f2 | f3 | f4 | f5 |
|---|---|---|---|---|---|---|---|
| 0 | Learning | 3 | 3 | 0 | 0 | 0.95 | 1.00 |
| 1 | Novice | 2 | 3 | 1 | 2 | 0.85 | 0.90 |
| 2 | Handy | 0 | 3 | 0 | 2 | 0.75 | 0.85 |
| 3 | Expert | 0 | 2 | 0 | 1 | 0.30 | 0.60 |
| 4 | Mekada | 0 | 1 | 0 | 1 | 0.05 | 0.30 |

Advancement (0x461467, every two seconds from the mechanic's tick 0x460635), one level at a time
(0x461536 only accepts current+1): exactly 9 completed jobs -> Novice; exactly 20 jobs, cash >=
15,000 or more than 4 cars -> Handy; cash >= 35,000 or more than 10 cars -> Expert; cash >=
120,000 -> Mekada. Each advance says "Congratulations! You just moved up from <old> skill to the
<new> skill level!" (`Skill Advance`) and, unless the new level is Novice, "You'll have access to
more cars in the Auction, and there may be new jobs to complete." (`Available Cars`).

## Parts and cars

A part (0x18 bytes, 0x43dde0): `+4 PartData, +8 condition (0 Scrap, 1 Barely, 2 Worn, 3 Perfect),
+0xC value wiggle, +0x10 its mesh, +0x14 its MULTIMAT`. A new part gets a wiggle of
`rand * 0.2 - 0.1`; a special-4 part is always Perfect. With `min`, `max` its costs and `c` its
condition:

| What | Where | Amount (never below 0) |
|---|---|---|
| value | 0x43e03a | 0 when Scrap, else `min + (max - min) * (1 + wiggle) * c / 3` |
| scrap price (the JunkYard pays) | 0x43e0bc | `min + ((max - min) * 2/3) * (1 + wiggle) * c / 3` |
| repair cost | 0x43e151 | `(max - value) / 2` |

A car in play (`Auto`, 0x50 bytes): `+4 AutoData, +8 the parts on it, +0x20 paint-on texture,
+0x30 repair time (ms), +0x34 repair cost, +0x38 purchase cost, +0x3C career number, +0x40 at Top
condition`.

Rules, each filling the mechanic's assembly error (`code`, up to 16 part ids), shown by 0x460b74 as
`Assembly Error!` (a part name ending in "s" reads as plural):

- Attach (0x44316e, 0x4431cc): a Scrap part fails with 4 ("That part's too messed up. It won't last
  ten minutes. You'll have to scrap it."). The same part already on fails with 1 ("There's already
  a[n] X on the car. Remove that first." / "some X ... those"). A part on the car sharing a mutex
  bit fails with 5 ("That part replaces the X, which is/are already attached."). Every attach
  dependency not on the car (with AMEA, one met by a mutex equivalent counts as on) is listed with
  2 ("You can't attach that part until the X is/are attached.").
- Remove (0x443344): every remove dependency on the car, or with a mutex equivalent on the car, is
  listed with 3 ("You can't remove that part until the X is/are taken off.").
- Assembled (0x443dc7, per region; 0 = all): no non-custom part of the region is off the car
  while it could go on (a slot filled by a mutex equivalent counts as filled).
- Worst condition (0x443eae, per region): the lowest condition among the parts on, 3 when none.
- A new car (0x45fbfd, 0x443628): with the skill's three ranges drawn as `centre = rand(f0..f1)`,
  `spread = rand(f2..f3)`, `share = rand(f4..f5)`, the non-custom parts are shuffled and put on
  (dependencies first, recursively) until `ftol(non-custom count * share)` are on; each gets the
  condition `ftol(lo + rand * (hi - lo))` with `lo = max(centre - spread, 0)`, `hi = min(centre +
  spread + 1, 3.99)`. The model's parts bank (0x45fe95) is then stocked for the JunkYard: every
  part not on the car (except special 4) at a random condition `ftol(rand * 3.999)`, plus
  `ftol(rand * 7)` random extra parts.
- Top condition (0x41048a, after every reframe): assembled with every part Perfect sets `+0x40`
  and says "Congratulations! You've fixed this baby up to Top Condition. And it only took you
  <time>. You spent a total of $<cost>." (`Car Complete`; not in a job, not for the dev mechanics).

Mechanic operations: remove (0x4602c2: the rules, `rattle\rattle12.wav`, car -> Parts Bin), attach
(0x46032f: Parts Bin -> car), repair (0x41b6a7 pays `ftol(cost)` without the till, then 0x46038e
plays `misc\repair.wav` to the end and sets condition 3), scrap (0x46020e: cash += scrap price,
the WorkShop car's repair cost -= `ftol(price)`, `misc\toilet2.wav` to the end, Parts Bin ->
JunkYard shelf), buy from the JunkYard (0x460193: pays the scrap price, shelf -> Parts Bin).

### How parts look

`TCondition` (0x458f20, four at 0x4f9848, `Scrap/Barely/Worn/Perfect`) holds a colour and three
materials: `+0x14` additive, unlit, in the condition colour (Show Condition and the part under the
wrench), `+0x54` flat (the Parts Bin frame and corner triangle), `+0x94` smooth, untextured, in a
"wear" colour (a part in any condition but Perfect is drawn all in it; Perfect parts get their
MULTIMAT materials back). Colours (0x4590a8, hardware renderer):

| Condition | Show Condition colour | Wear colour (0-255) |
|---|---|---|
| 0 Scrap | 0.45 0.45 0.45 | 71 49 38 |
| 1 Barely | 0.9 0.1 0.1 | 85 78 67 |
| 2 Worn | 0.9 0.9 0.1 | 153 134 128 |
| 3 Perfect | 0.2 0.9 0.2 | 255 255 255 |

What shows (0x443a8d, for view `v`): each part on the car whose region is `v` (in the Complete
view, each with VIC), its shine mesh only when Perfect; each non-part mesh whose name has `0` as
its second character (`$01`, `#0...`) when `v` is Complete or the name starts with `"0#$^"[v]`,
with its paint polygons on the car's paint.

## The WorkShop

`WorkShop` (0xC83C bytes, 0x41d91f) is made of: `+0x38` the 3D view (0x40e2e0), `+0x7D8` the Parts
Bin with Repair and Scrap (0x41b220), `+0xA44` the view buttons (0x415370), `+0xBFC` the right
column (0x413e90), `+0xE14` the places (0x414a00), `+0xFCC` the tools (0x41c9c0), `+0x133C` the
Decal Browser, `+0x198C` the decal set up, `+0x19BC` the camera, `+0x1A00` the view (0 Complete,
1 Engine, 2 Body, 3 Running Gear),
`+0x1A14` the spray can, `+0xC67C` the wrench.

- View buttons (screen rects in `WorkShop.tga`): Complete (3,0x2E)-(0x7C,0x4C), Engine
  (0x8B,0x2E)-(0xE5,0x4C), Body (0xF4,0x2E)-(0x13B,0x4C), Running Gear (0x14A,0x2E)-(0x1F8,0x4C);
  changing view (0x415710) refreshes the car and the Parts Bin (filtered to the view's region;
  all parts in Complete), reframes, and plays the view's sound. A job enables only its views.
- Right column: Show Condition (0x20A,0x67), Put Car In Lot (0x93), Get A Job (0xBF), Auction Car
  (0xEB), Catalog (0x117), each to x 0x280, 0x1E high. No car: Show Condition, Put Car In Lot,
  Auction Car and Catalog are disabled; in a job: Put Car In Lot, Auction Car, Get A Job; the dev
  mechanic: Get A Job. Show Condition works while held (`misc\condition1.wav` down,
  `condition2.wav` up): every part on the car in its additive condition colour, the `?0` meshes
  hidden.
- Places: Go To Car Lot (0x148), Go To JunkYard (0x174), Go To Auction (0x1A0), Exit
  (0x24F,0x1C8)-(0x280,0x1DD). No car: JunkYard disabled; in a job: Car Lot and Auction. Go To
  Auction with 12 cars says "Your Car Lot is full..."; Exit in a job asks "If you leave now,
  you'll have to restart this job..." (`Job Active`).
- Tools (0x41cb1f): Impact Wrench (0x191,0x61)-(0x1EA,0xA1), Body Paint (0x191,0xA5)-(0x1EA,0xE5)
  sharing its place with the Camera (`Overlays/CameraUP/DN.tga`), Model in Photo
  (0x190,0xEC)-(0x1F0,0x111), Start Engine (0x191,0x122)-(0x1EA,0x162). By view (0x41cd57):
  Complete: wrench, camera, model in photo (when the car has a model), start engine; Engine:
  wrench, start engine; Body: wrench, body paint; Running Gear: wrench. In a job: the wrench, and
  start engine in Complete and Engine. Start Engine without sound says "Starting the engine
  requires working sound drivers".
- The 3D view: (5,0x55)-(0x181,0x156); the status line (0x55,0x159)-(0x181,0x171) in Standard;
  `Overlays\UseKeys.tga` top left; the `Assembled` tag (`AssemR/Y/G`, by `ftol(worst - 0.5)`) top
  right while the view's region is assembled.
- Camera: arrow keys turn 5 radians a second, the polar angle kept in 0.3927..2.7489; right drag
  turns; the camera eases 3/4 of the way each tick. Focus on a part (0x40fd2c): looks at the mean
  of the part's position and its bolts, from its farthest vertex's distance over tan(22.5
  degrees). The original never lets go of that focus (`+0x79C` is set, never cleared, and the
  reframe 0x40f2b1 keeps it), so after the first bolted part in the Body or Running Gear view the
  camera stays there; the rebuild reframes the car when the wrench's job ends or the view changes.
- Start Engine (0x443f40): no special-2 part on: the car's no-start sound; engine not assembled or
  its worst part Barely or Scrap: the failed-crank sound; otherwise it runs: the ignition sounds of
  the parts on the car that have one (or the car's default), the engine parts shake by up to 0.4
  and the special-3 parts spin 0.628 radians a tick (0x44409c) while the sound plays.

### The Parts Bin (partlist.cpp)

The bin (0x44085c) lays 8 `PartButton`s over (5,0x172)-(0x181,0x1DB): 4 columns by 2 rows, 2
pixels apart, each `(w - 5*2)/4` by `(h - 3*2)/2`. It lists the WorkShop car's bank's Parts Bin
records in the view's region, 8 a page; the page number sits between the arrows
(0x186,0x19D)-(0x19A,0x1B1) in TinyWhite; up (0x185,0x172)-(0x19B,0x193), down
(0x185,0x1BA)-(0x19B,0x1DB). Each cell (0x43ec78) draws its part's mesh lit, alone, on black, from
`radius / tan(pi/4)` away, light at (-50, 10, -25), in its MULTIMAT materials when Perfect and in
the wear colour otherwise; the name centred at the bottom in TinyWhite; a 13-pixel triangle in the
condition colour in the top left corner; and a frame in the condition colour while hovered or
while the wrench points at a part with the same id. The hovered cell's part spins 4 radians a
second.

Pressing a cell picks up a copy of it (1 pixel larger) that follows the pointer until release
(0x45b860); the drop (0x41b6a7):

- on the 3D view: switches to the part's region view if needed (fails in a job without that
  view), picks the wrench, and starts attaching;
- on Repair (0x19E,0x173)-(0x1F9,0x1A5): Perfect: "That part's in perfect condition!"; Scrap:
  "That part's too messed up to repair. The best you can do is scrap it and get a few bucks from
  the Junkyard."; otherwise "It will cost $%4.2f to fix up this <name>. Do you want to repair it?" (`Repair
  Part`, Yes/No), then pays `ftol(cost)` and repairs;
- on Scrap (0x19E,0x1A9)-(0x1F9,0x1DB): "The JunkYard will pay $%4.2f for this <name>. Do you want
  to scrap it?" (`Scrap Part`), then scraps.

Clicking Repair or Scrap themselves explains: "Drag parts from the Parts Bin onto the Repair
Square to repair them." / "...Scrap Square to scrap them." (`Repair Square`, `Scrap Square`).

### The wrench (Air Ratchet / Impact Wrench)

States (`+0x1A4`): 0 pointing, 1 unscrewing, 2 screwing. Pointer `Pointers\Wrench.tga`. Sounds:
`ratchet\ratchet23.wav` (a bolt in), `ratchet24.wav` (a bolt out), `button\button2.wav` (attach
starts), `misc\magnet3.wav` (remove starts), `air\air7.wav` (a miss).

- Pointing (0x4204cc, not in the Complete view): the part under the pointer turns to its additive
  condition colour, its name shows on the status line, and Parts Bin cells with the same part id
  get their frame; moving off puts its look back.
- Clicking a part (0x41fada): the remove rules (an Assembly Error otherwise). A part without bolts
  comes straight off; otherwise each bolt shows (white), the camera focuses on the part (not in
  the Engine view), and the tool dialog opens at (390,95): `Impact Wrench Tool`, "Use the left
  mouse button to unscrew the bolts.", Cancel.
- Dropping a part from the bin (0x41fc1f): the attach rules; without bolts it goes straight on;
  otherwise its mesh shows, its bolts show (black), and the dialog says "Use the left mouse button
  to attach the bolts."
- A bolt (0x420231): a small cube (size 2, scaled 1.5) at `PD_BOLT` plus an additive billboard of
  `Overlays\BoltArrow.tga` (a line up and right to the word BOLT) of size 15. White: in; black:
  out; orange (1, 0.75, 0): under the pointer and clickable. Clicking one turns it (out: black,
  its label gone; in: white); the last one finishes the job (0x41f8dd): the part moves, the dialog
  closes, the car, the bin and the camera refresh.
- Cancel (0x41336d): the bolts already turned are turned back (their sounds played to the end)
  and everything is put back.

## The Catalog (catalog.cpp)

Art `Catalog.tga` (the page and disabled look), `CatalogUP.tga`, `CatalogDN.tga`; `CatalogDC.tga`
for the decal pages. Go Back To WorkShop (0x14C,0x30)-(0x1F3,0x51); Turn Page forward
(0x200,0x1B4)-(0x255,0x1C1) and back (0x2A,0x1B4)-(0x7B,0x1C1), `catalog\catalog19.wav`. Tabs
(0x423713, `catalog\catalog9.wav` on press): Engine (0x23F,0x79)-(0x270,0xE6), Body
(0x244,0xEF)-(0x274,0x13A), Running Gear (0x248,0x142)-(0x27A,0x1A9), Decals (0xE,0x7A)-(0x40,0xE8).
It opens on the WorkShop view's region (Body for Complete), or Decals when the spray can is out.

The pages (0x4221c5): a PartList in catalog mode over the left page (0x3A,0x69)-(0x134,0x1B4) and
the right page (0x14B,0x69)-(0x245,0x1B4), each two columns of five, 15 pixels apart; the left
page's ten first. Every part of the WorkShop car's model in the region is listed (in a job, only
those the job allows), each on a grey card (0.69, 0.67, 0.65) with its name in TinyBlack, the
hovered one turning. Clicking one (0x422b7c): "This <name> will cost you $<cost max>. Do you want to
buy it?" (`Buy Part`); yes pays `ftol(cost max)` with the till and puts a new Perfect part in the
model's Parts Bin. Decals (0x422d74): "<n> "<name>" decals cost $<price>. Do you want to buy
them?" (`Buy Decal`).

## The JunkYard (junkyard.cpp, yardarea.cpp, yardpane.cpp)

Art `JunkYardUP.tga` (background) and `JunkYardDN.tga`. Go Back To WorkShop (0x14C,0x30)-(0x1F3,0x51)
(leaving rings the till if anything was bought); walk left (0xC,0x13B)-(0x2E,0x15C) and right
(0x1D8,0x13B)-(0x1FA,0x15C) while held; the area signs Engine (0x1F3,0x8D)-(0x266,0xAC), Body
(0x1EF,0xAC)-(0x260,0xCC), Running Gear (0x1EA,0xD1)-(0x26B,0xEF); the Purchase Bin (a PartList of
6 by 2 over (0xB,0x171)-(0x258,0x1DA)) with its page arrows (0x25E,0x16C)-(0x274,0x18D) and
(0x25E,0x1B9)-(0x274,0x1DA). It opens on the WorkShop view's area (Engine for Complete).

The 3D view (0x42e8c0) is (0x33,0x5F)-(0x1D5,0x15C); the label under it (0x78,0x15D)-(0x255,0x171)
says "<name>  ($<scrap price>)" in Standard for the part under the pointer, which turns to its
additive condition colour. Each area is a scene: `Scenes\JYEngine.3ds`, `JYBody.3ds`,
`JYRGear.3ds`. The camera starts at `play_01` looking at `CamFocus` and, while an arrow is held,
moves 500 units a second along the camera's right axis projected on `play_01`-`play_02`, stopped at
the ends (0x46d5e8), with a head bob and `misc\footstep1.wav` steps (0x46d7dc).

Shelves (0x42de39, 0x42d406): the flat meshes `Shelf01`, `Shelf02`, ...; each is a line along its
longer horizontal side, starting 15 units in. The bank's JunkYard records of the area's region
(0x42ee87) go on in order (0x42d813): a part is turned so its longer horizontal side lies along the
shelf, placed `radius + 10` after the one before (its own radius from its start), resting 2 units
above the shelf; the first part that does not fit closes that shelf and the rest go on the next.

Buying (0x42c3fa): clicking a shelf part pays the scrap price (0x460193, no till), moves the record
from the shelf to the Parts Bin, hides it on the shelf and puts it in the Purchase Bin
(`rattle\rattle12.wav`). Clicking it in the Purchase Bin (0x42c4b4) pays it back
(`spend(ftol(-price))`) and returns it to the shelf.

The scenes are 3D Studio files, Z up; the walking places take the scene camera's field of view
(45 degrees in the JunkYard and the Car Lot, 30 at the Auction) across the view's height.

## The Car Lot (carlot.cpp, lotpane.cpp)

Art `CarLot.tga`, `CarLotDN.tga`. Go Back To WorkShop (0x14C,0x30)-(0x1F3,0x51); walk left
(0x1F,0x160)-(0x41,0x181) and right (0x236,0x160)-(0x258,0x181) while held. The view (0x429a70) is
(0x53,0x61)-(0x224,0x181): `Scenes\CarLot.3ds` with its `Floor` object switched off, `Scenes\Clouds.jpg`
over the top 35% of the view behind everything, the walk from `play_01`. The mechanic's cars stand on
the spots `car_01`..`car_12` in the order they are kept (0x428971; the WorkShop's car leaves its spot
empty), each drawn as in the Complete view without shine, turned as its spot (the spots' frames
carry a 0.19 scale the cars do not take), resting on the spot's base, with a half see-through black
shadow of its outline a unit above the ground (0x42a6d6). The car whose spot projects nearest the
view's middle (0x42acc9) gets `Overlays\CarArrow.tga` (32 pixels) on the bar under the view and its
record under it (0x428cb4): number (CashFont), `$ ` purchase cost (CashFont), repair time as
`hh:mm:ss` (Standard), `$ ` repair cost (CashFont) in (0x39,0x1A8)-(0xA2,0x1C3),
(0xC2,0x1A8)-(0x12B,0x1C3), (0x14C,0x1A8)-(0x1B5,0x1C3), (0x1D5,0x1A8)-(0x23E,0x1C3). A click on the
view (0x428f62) makes that car the WorkShop's, in the Complete view. Sounds: `ambient\field1.wav`
looped, `crow1`/`crow2` in turn and `cardepart` at random intervals (0x428b94). Put Car In Lot in the
WorkShop (0x4145ed) empties the WorkShop and goes there.

## The Auction (auction.cpp, bidpane.cpp)

Art `Auction.tga`, `AuctionUP.tga`, `AuctionDN.tga`. Go Back To WorkShop (0x14C,0x30)-(0x1F3,0x51),
Place Bid at this Asking Price (0x204,0x14A)-(0x279,0x17E), Skip Car (0x204,0x193)-(0x279,0x1B2);
`misc\chatter.wav` looped. The view (0x4274e0) is (0xC,0x5F)-(0x200,0x11D): `Scenes\Auction.3ds`
seen through its camera, `Clouds.jpg` behind, the car standing on the `Floor` object; it drives in
from 400 units past the `BoundBox`'s right side, closing 15% of the gap each 10 ms tick, and out to
the left gaining 10 units and 15% a tick (0x427c5d). The painted stage below holds the clock:
`Overlays\Timer.tga`, sixteen 23-pixel pictures from all red to none, at (0x10A,0x126).

- Buying (Go To Auction, refused with 12 cars): each car is a model the mechanic's skill allows
  (`CD_MINSKILL`; any for the dev mechanics), rolled new (0x423f54) and painted one of the 27 colours
  of `Gfx24\PaintColors.tga` (0x442c96). Skip Car sends it off for the next.
- Selling (Auction Car: "You are about to auction your car. Once it's on the block, the only way to
  get it back is to bid on it. Do you want to continue?"): the WorkShop's car; the player cannot
  leave or skip.
- Worth (0x4258cc): `low` = the sum of the part values times the average condition (the sum of the
  conditions over count - 1, over 3) times 1.02 - rand 0.04, plus 15% of the values of each region
  that is assembled and 25% of the values of the custom parts; `high` = `low` + 20% of (the model's
  summed top costs - `low`), at least `low` + 100.
- Six bidders, made once with a keenness of 0.5 + rand 0.5 (0x4252e0); per car each gets a floor
  `low*0.75 + low*keen*0.1`, a ceiling `high*0.75 + high*keen*0.25` and a whim of rand 1 (0x426608).
- The bidding (0x426060): the current bid starts at 100; the asking price is `current + high/18`
  (0x425c8e); every 2 s it drops by 25, not below `current + 25`; every 750 ms each bidder (not the
  last to bid) wants to when `rand(willing + whim) > 1`, with `willing` 1 up to its floor falling to
  0 at its ceiling (0x426826); the least keen of those who want to bids (a `auction\bid*.wav` voice,
  the nine in a shuffled order), the current bid becomes the asking price. The player's bid
  (0x425ce8) needs the cash for the asking price and makes it the current and "your" bid; ">>" then
  shows by the current bid. The clock runs twenty seconds; the prices are shown and paid rounded down
  to fives (0x425619): Current Bid (0x207,0x7D)-(0x26F,0x99), Asking Price (0x207,0xCD)-(0x26F,0xE9),
  Your Bid (0x207,0x11B)-(0x26F,0x137), CashFont right-aligned.
- The end (0x424068): buying, the player's top bid wins the car (0x460431: it goes into the WorkShop
  with the next career number, its price as the purchase cost; "You've won the bid for this car!",
  `Winning Bid!`), anyone else's sends the car off. Selling, the player's top bid cancels the sale
  for a service fee of 5% ("You've overridden the auction. You were charged a service fee of $%i.",
  `Sale Cancelled`); otherwise it is sold (0x4603e2) with "Congratulations. Somebody just shelled out
  $<price> for your car." and " You lost $x." (" Pick another hobby." below -5,000, " Ouch!" below
  -1,000) or " You made a profit of $x." (" Man alive!" above 5,000, " Wow!" above 1,000), the profit
  being the price less the repair and purchase costs (`Sold!`).

## Jobs (jobengine.cpp, job.cpp, globjob.cpp, jobgen.cpp)

The job files (`Data\Jobs\*.jpk`, read in folder order at start, 0x44bcb8): `JOBFILE` holds the
portraits (`JOBPHASEIMAGE`, 75x75 24-bit BITMAPs, numbered in file order) and `JOBDATA` records
(0x44d0d4; the job is 0x7C bytes, made by 0x44c49a): `JOB_ID` (+0), `JOB_CARID` (+4), the file
(+8, -1 for a made-up job), the customer's car while running (+0xC), `JOB_ANXIETY` (+0x10, read,
never used), `JOB_FEE` (+0x14, float), `JOB_CARCOLOR` (+0x18, three floats 0..1: the paint),
`JOB_CARBITMAP` (+0x24, a paint picture; no retail job has one), `JOB_LEGALMODES` (+0x28, four
int32 flags: Complete, Engine, Body, Running Gear), `JOB_ALLOWCUSTOMPARTS` (+0x2C), the next
phase (+0x30), `JOB_PHASE`s (+0x34: `JP_TYPE` 0 request, 1 hint, 2 thanks; `JP_TEXT`;
`JP_IMAGEINDEX`), `JOB_STARTSTAT`s (+0x4C) and `JOB_COMPLETESTAT`s (+0x64): `JS_TYPE` 0 a part /
1 a region, `JS_SUBJECT` the part id or region (0 the whole car), `JS_CONDITION`.
`JOB_ILLEGALPART` is named but never read. A new job's defaults: id 0x7FF, a paint from the 27
colours, every view open, custom parts allowed.

- Start stats (0x4521d0), in order, on a car with no parts: a region stat puts on every part of
  the region (not custom, not special 4) at the condition, in the car file's order, until the
  region is assembled (0x44345a with spread 0: dependencies first, the same condition). A part
  stat sets an attached part's condition, or throws it away for -1 (0x443859: the parts that must
  come off first go too), or puts a missing part on at the condition (custom allowed).
- Complete stats (0x452133): a part on the car at the condition or better (-1: any); a region
  assembled with its worst part at the condition or better. All met (checked every WorkShop tick,
  0x44ed95) finishes the job; so does holding scan codes 0x49 and 0x52 (Page Up and Insert), a
  developer's key.
- A phase of a kind (0x44d006) is searched from the one after the last given, going round: Job
  Help cycles through the hints.
- Views: a closed view's button is disabled (0x41562d), a drop from the Parts Bin needing it is
  refused (0x420eaf), Show Condition hides its parts (0x41427c). The Catalog (0x422443) and the
  JunkYard shelves (0x42ee87) offer custom parts only when the job allows them (0x44cfd8).

Get A Job (0x41461d -> 0x44ee7c): with `-JobID n` that job; otherwise the job with the lowest fee
(0x44be5c) not yet done whose car's `CD_MINSKILL` the skill reaches; otherwise, unless the
mechanic's automatic request is on, a made-up job; otherwise "There are no more jobs to complete
right now. Try buying, fixing, and re-selling cars from the auction to make money." (`No More
Jobs.`, not for the dev mechanics) and the automatic request goes off. The portraits load
(0x44c8eb), `Job\bepbeep.wav`, and the request shows (0x44e579).

A new mechanic's automatic request is on (0x45ee3f, `MECH_AUTOJOBREQUEST`): with no job running,
the WorkShop tick offers the next job by itself, with OK only (no Cancel), and Job Help offers
Restart instead of Cancel. It goes off when the ninth job is done (0x460ecc), after the first has
said "Way to go. You've just completed your first job as a Virtual Mechanic." (`Congratulations`)
and "With any luck, more jobs will roll in until you can afford to buy your own cars." (`The
Future`). The nine tutorial jobs (fees 100..300) are the cheapest, so they come first.

The job dialog (0x44e103, 0x44e374): `Overlays\Job\JobFrame.tga` at (0xAF,0x5A), opened without
dialog8.wav; relative to it the portrait (0xE,0x22)-(0x59,0x6D), the customer's words
(0x66,0x36)-(0x126,0xE5), the difficulty (0xE,0x8D)-(0x59,0xA6), the fee "$%4.2f"
(0xE,0xC7)-(0x59,0xE0) and a line (0xC,0xE8)-(0x120,0xFE), all in Standard, centred from the top;
OK at (0x42,0x106), Cancel or Restart at (0xB5,0x106). Difficulty by fee: under 250 Easy, under
550 Medium, under 1050 Hard, else Expert. `Job Request` (the request phase, "Hit OK to begin the
job."), `Job Update` (the next hint, "Hit OK to continue or CANCEL to give up on this job." /
"...RESTART to restart this job."), `Job Complete!` (the thanks, no line).

Starting (0x44f061, 0x44c722): the customer's car is made (a new car of the model, its 256x256
paint filled with the job's colour, then the start stats); the mechanic is saved as they are
(0x45f631); the car goes into the WorkShop (its model's parts bank made and stocked if new,
0x45fda8: the job shares the mechanic's bank for that model), the cash becomes the fee (the
budget; the cash before, rounded down, is kept at +0x348), the Complete view, and the tab: a
button of `Overlays\Job\JobTab.tga` (118x118) at (0x20A,0x93) over Put Car In Lot, Get A Job and
Auction Car, the request's portrait on it at (0x15,0x21). Clicking it (0x44eff4): `Job\com.wav`,
Job Update; Cancel/Restart gives up. In a job the mechanic is never saved (0x45fb42), Put Car In
Lot, Get A Job, Auction Car, Go To Car Lot and Go To Auction are off, and Exit asks "If you leave
now, you'll have to restart this job when you return!" (`Job Active`) and signs out without
saving.

Done (0x44f141 with success): the tab shows the thanks portrait, the engine starts (0x443f40), the
JukeBox plays its next victory tune (`music\victory2.wav`, `victory3.wav` in turn, 0x45c933),
and for three seconds (0x44f2fa) only the 3D view runs: the camera's polar angle goes to 1.2566
and it turns as the right arrow does. Then `misc\cashreg.wav`, Job Complete!, the engine stops
(0x444472), cash = the kept cash + the fee + what is left of the budget, the job counts (0x460ecc),
the mechanic is saved, and the WorkShop is left empty (the car the mechanic had there before is
back in the Car Lot). Giving up (0x44f141 otherwise) reads the mechanic back from the file
(0x45f859), so everything is as it was before the job.

### Made-up jobs (jobgen.cpp)

0x44f98d, the first time: `Jobs\random\Comments.txt` is read (0x40608d), then every phrase list is
moved on `ftol(rand(20))` times (0x40680a). Each job: a new job record (a random paint), a car
drawn uniformly from all the cars until one the skill allows (a thousand tries), a scratch car of
it made whole and perfect (a region-0 condition-3 stat, which is also the only complete stat), its
parts counted by region, then:

- The damage (0x44fd37): the views but Complete close; a start stat region 0 condition 3; the
  places: the whole car if `rand(1) > 0.8`, each region if `rand(1) > 0.5` (drawn in the order
  whole, body, engine, running gear; the whole car when none). Per place `ftol(rand(2.999) + 1)`
  spots of `ftol(rand(4.999) + 1)` parts (three times both for the whole car, which ends the
  list). A spot (0x4506b7) is a random part of the place (a thousand tries for one not yet
  picked); the parts nearest it (by their meshes' places, 0x450547), not already picked and in
  the place, are picked, up to the size (at least one, at most all but one). Each picked part
  gets `c = ftol(rand(4.999) - 1.999)` (-1 gone, 0, 1, 2); a gone one becomes `ftol(rand(2.999))`
  one time in four, a scrapped one `ftol(rand(1.999) + 1)` one time in two, and any part in a
  region that already has a scrapped part stat becomes 1. The part stat is added; a gone part is
  thrown away with the parts that had to come off first (0x450315), each counting as gone, adding
  its top cost to the fee and opening its view; a scrapped part adds its top cost, a damaged one
  its repair cost; either opens its view. The fee is then times 1.25.
- The words (0x450933, 0x450c8c): with at most two parts damaged or gone, a `PartDam` sentence per
  damaged part and a `PartGone` per gone part (`%PartName` its name), joined by `%And`, with one
  more `%And` before the first gone part; with more in one region, a `RegionPerc` sentence; else,
  Symptom or Cause first at even odds, a `<first>: <region>` sentence (the other kind when there
  is none) for the whole car when all three regions are hurt, or per hurt region joined by
  `%And`. The request is PreReq (one time in two) + that + PostReq (one time in two); the one hint
  is that again; the thanks is `Thank`. The portraits (0x44c8eb) are `Face: Sad` (every phase but
  the thanks) and `Face: Happy`, from `Jobs\random\`, scaled to 75x75.

The phrase book (RatDepot, 0x40608d): `;` comments, `%Name = Type: "a", "b", ...` variables, and
`Key: [Key:] "text"` entries. A list hands its texts out in turn (0x406c32: the index moves on,
then the text), which keeps the 28 Face/PreReq/PostReq/Thank lists in step: one customer each.
Expanding (0x405d77): each variable found is replaced everywhere by one choice: `Random`
`ftol(rand(n))`, `CondRange` `ftol(n * cond)`, `PercRange` `ftol(n * share)`, `RegionName`
`ftol(region / 4 * n)`, clamped; `cond` is the region's worst condition for RegionPerc but
3 - worst for Symptom/Cause (so it nearly always takes the last choice), `share` the region's
damaged parts over its parts, `region` the only hurt region or 0; the values stay from one job to
the next. Then (0x405eab) each `.` `!` `?` gets a space after it, the text is trimmed, and each
sentence starts with a capital with the rest in lower case. "Sympton: Engine:" in the file is a
misspelt key, never used.

## Body Paint (spraycan.cpp, paintpane.cpp)

The tools are a radio group in the WorkShop's tools column; the chosen one gets the 3D view's
pointer events (a vtable: moved, pressed, released, held each tick). Body Paint (Body view, not in
a job) is the spray can (`+0x1A14`, 0xAC68 bytes, 0x416995): pointer `Pointers\paint.tga` (the
nozzle at its top left), `air\spray.wav` looped, `misc\bead1.wav` / `bead2.wav`.

The panel (0x419579, 0x4193b0) replaces Model in Photo and Start Engine: (0x183,0xE3)-(0x1FA,0x16A).
Four brush buttons 22x22, 3 apart, centred along the top at y 0xE6 (x 396, 421, 446, 471):
`Pointers\Brushes\Small/Medium/Large/Part.tga` (24x24) scaled down, a (0.99,0.99,0) 2-pixel frame
when chosen. The 27 colours of `Gfx24\PaintColors.tga` (the table at 0x4fbb30, filled by 0x4659b6)
3 across and 9 down in (0x183,0xFF)-(0x1FA,0x156), 2 apart (37x7 each), a (0.99,0.99,0.99) frame
when chosen (0x41a054). `Overlays/DecalsUP/DN.tga` at (0x196,0x153) opens the Decal Browser. The
first brush and colour are taken when none is (0x419c12); choosing either puts a decal down.

- Moving over the view (0x416ab7), without a decal: a jump of 17 pixels or more down plays bead1
  (the can shaken), then one up bead2, in turn.
- Held (0x416f10, each 10 ms tick), without a decal and not blocked: the hiss starts if silent;
  every pixel of the brush picture with r+g+b <= 0x40 is painted at the pointer less half the
  picture plus the pixel, with a square of radius 0/1/2 texels (Small/Medium/Large) round the
  texel it hits (0x417678). Part fills the whole box of texture the picked part's paint covers.
  Released: the hiss stops.
- Painting a screen point (0x417678): the paint polygons (material 666, the car's paint-on
  texture) of the shown meshes that face the camera (their normal against the view direction) are
  projected and binned into a 12x12 grid over the box they cover, one cell spare each way,
  rebuilt when the camera moves (0x415e85). The point and the point + (0.5,0.5) each cast a ray
  against the cell's polygons (64 at most); the nearest hit's UV (barycentric; below 0 wrapped by
  +1 after clamping at -1, above 1 clamped) picks the texel. A car without paint polygons (many
  fan cars) cannot be painted. The texture is uploaded after each tick.
- Pressed with a decal (0x416b9f): every pixel of the set-up decal whose (r+g+b)/3 >= 8 paints one
  texel at the pointer less half the decal plus the pixel, in its own colour; one copy is used
  (0x460767); with the last gone the can drops the decal and holding does nothing until the
  button is let go (+0xAC60).

## Decals (globdecal.cpp, decal.cpp, dbrowser.cpp)

`Data\Decals\*.dpk` (folder order; "Must have at least one decaldata loaded."): `DECALFILE` >
`DECALDATA`: `DD_ID`, `DD_NAME`, `DD_COST` (a pack), `DD_NUMPERBUY`, `DD_COLORTYPE` (0 as drawn,
1 one colour), `DD_IMAGE` (a 24-bit BITMAP; black is see-through). The retail set has 75 in five
files. A mechanic keeps `MECH_DECAL` records (`DECAL_ID`, `DECAL_COPIES`).

The Catalog's Decals tab (0x4221c5): `CatalogDC.tga` as the page, all decals on a decal grid over
the two pages (0x3A,0x69)+(0xFA,0x14B) and (0x14B,0x69)+(0xFA,0x14B), 16 a page; it opens there when
the spray can is out. A click (0x422d74): "<n> "<name>" decals cost $<cost>. Do you want to buy
them?" (`Buy Decal`, Yes/No), then 0x4607ef pays with the till and adds n copies.

The decal grid (0x4359a4): cells 48x72, 8 apart; columns and rows as fit (width / 56, height /
80), the block centred (x from left + w/2 - cols*56/2 - 4, then +8). A cell (0x4356b1): the decal
at 48x48, its name in TinyWhite under it ((0,0x30)-(0x30,0x48)), and in the browser the copies
left, TinyWhite, at the cell's left middle.

The Decal Browser (0x433ee6) is a window at (0x78,0x5A) (400x300). "Choose a Decal:"
(`DBrowser1.tga`, buttons from `DBrowser1UP/DN`): the mechanic's decals on a grid over
(0xD,0x31)-(0x16B,0xF9) (6x2), up (0x16F,0x31)-(0x185,0x52) and down (0x16F,0xD8)-(0x185,0xF9),
Cancel (0x12D,0x105)-(0x185,0x126), and Last Used (8,0x10A)-(0x48,0x122) when a decal was set up
before; with none, "You don't have any Decals! Go to the Decals section of the Catalog to purchase
some." in Standard. A decal clicked is set up (0x432d13: as drawn, upright, full size) and "Modify
Decal:" shows (`DBrowser2UP.tga`): TURN (0x13,0x41)-(0x43,0x6B) left and (0x94,0x41)-(0xC4,0x6B)
right, FLIP (0x13,0x9A)-(0x29,0xDA) top to bottom and (0x2B,0xDD)-(0x6B,0xF3) side to side, BIGGER
(0xAE,0x83)-(0xE3,0x99) 1.5, Normal (0xAE,0xB1) 1, smaller (0xAE,0xDD) 0.5 (the size's button held
down), Back, DONE (0xC3,0x105)-(0x11B,0x126), CANCEL; for a one-colour decal the 27 colours in
(0xFF,0x42)-(0x17B,0xEF), 3 apart; the decal stretched over the box (0x3C,0x6B)-(0x98,0xC7).

Setting up (0x432c17..0x4337e1): a colour (black taken as 0.1 grey) redoes the picture from the
file with each pixel of a one-colour decal `ftol(grey * colour)` (grey = (r+g+b)/3), then the
turns and flips again (kept as one of the eight orientations, 0x4337bb); the stamp is that sized
(0x432d58); the pointer shows a copy with only the pixels at odd x and odd y (a see-through
look), centred. Done hands it to the spray can; Cancel drops it.

## Photos (camera.cpp)

The camera (`+0x19BC`, 0x40ce70; Complete view, not in a job) shares Body Paint's place
(`Overlays/CameraUP/DN.tga`), pointer `Pointers\camera.tga`. Model in Photo (0x190,0xEC)-(0x1F0,0x111)
shows when the car has a mesh called "model" (0x4462e3; twelve retail cars: a figure by the car)
and toggles whether it stands in the photos. A press on the view (0x40d172): `misc\camera.wav`;
the view is drawn over the whole 640x480 screen on (0.99,0.99,0.99), the figure shown if asked,
`Overlays\MekadaLogo.tga` (128x213, black see-through) at (5,5), read back (0x40d04b) and written
(0x40d626) to `Data\SnapShots\Shots_<mechanic>\Shot<n>.jpg` (quality 75, the first n below 1000
free) with a 100x75 thumbnail `Shot<n>.bmp` (8-bit in the original); `-PrintableSnaps` writes
`Shot<n>p.bmp` from a pure white one instead. 0x40d989 and 0x40dc2b (an HTML parts list coloured
by condition) are never called.

## Music and the loading screen (jukebox.cpp, video.cpp)

The JukeBox (0x45c9a5) holds three loops, `music\loop1.wav`, `loop2.wav`, `loop4.wav`, and two
victory tunes, `victory2.wav`, `victory3.wav`. Playing a loop (0x45c8c4) stops the one playing and
loops the new one; 0x45c888 stops it. Start-up (0x45d18e) shows the loading screen (the "Video",
0x4696a1: `Splash\Loading1.tga` with `misc\gamego.wav`) with loop 1 (loop2.wav) while the data
loads; the sign-in sheet (0x430a51) plays loop 0 (loop1.wav); the credits (0x459c19) loop 2
(loop4.wav); signing in (0x4317b8) and the WorkShop (0x41db96) stop the music, so the WorkShop and
the places have only their own sounds. A victory tune (0x45c933: the last cut short, the next of
the two in turn) plays when a job is done and when a car reaches Top condition (0x4609f3). The
WorkShop camera's zoom onto a part plays `misc\zoom1.wav` (0x40fd2c). `rattle\rattle1.wav` is
loaded by the Parts Bin (0x41b4e1) and never played; the `CDPlayUP/DN.tga` art is not used by the
game. With a developers' mechanic (-CarID, -JobID), leaving the WorkShop ends the game (0x430a51).

## The launcher (Gearhead.exe)

The MFC launcher started `ghg.exe -MustBeHere` (with `-Vid` for the display mode: "Auto -
Attempts to find the best display mode", "Direct3D - Requires DirectX 7 and 3D video card",
"Software - Requires DirectX 7") and held the Gearhead Garage SnapShot Browser ("Whose Snapshots
would you like to view?", view, "Are you sure you want to delete this Snapshot forever?", print,
e-mail through MAPI) and the updater (`mekupd.dll`). The rebuild has no launcher: its Settings sheet
does this inside the game. `Overlays\SignIn\SetUP.tga` / `SetDN.tga` (a SETTINGS button in the
sign-in sheet's style) ship in `Gfx24.dat` but no code of ghg.exe names them.

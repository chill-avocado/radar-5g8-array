"""SYSTEM.png -- one sheet showing how every part of the radar fits together.

The stack is the real OpenSCAD assembly (mech/stack3d.png) and every board is
its own copper artwork, not a stand-in.
"""
import json, matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.image as mpimg
from matplotlib.patches import FancyBboxPatch

D = 'design/'
B = {k: json.load(open(D + f)) for k, f in (
     ('tx', 'board_transmit.json'), ('rx', 'board_receive.json'),
     ('fe', 'fe_board.json'), ('pa', 'pa_board.json'),
     ('lna', 'lna_board.json'))}
IM = {k: mpimg.imread(D + 'brd_%s.png' % k) for k in B}
STACK = mpimg.imread('mech/stack3d.png')

INK, GREY, BLUE, RED = '#1b1b1b', '#6b7280', '#1d4d8f', '#a52a2a'
W, H, DPI = 19.0, 29.0, 100
fig = plt.figure(figsize=(W, H), facecolor='white')
fig.suptitle("5.8 GHz DRONE RADAR   —   HOW EVERY PIECE FITS TOGETHER",
             fontsize=33, fontweight='bold', y=0.987)


def panel(rect, xr, yr, equal=False):
    a = fig.add_axes(rect); a.axis('off')
    a.set_xlim(*xr); a.set_ylim(*yr)
    if equal:
        a.set_aspect('equal')
    return a


def place(ax, key, x, ytop, hgt, sx, sy):
    """Drop a board image in with its true proportions."""
    im = IM[key]
    w = hgt * (im.shape[1] / im.shape[0]) * (sy / sx)
    ax.imshow(im, extent=[x, x + w, ytop - hgt, ytop], zorder=5)
    return w


# ------------------------------------------------- the stack, and its labels
axs = panel([0.020, 0.452, 0.960, 0.513], (-0.14, 1.14), (0, 1.02), equal=True)
AR = STACK.shape[1] / STACK.shape[0]
axs.imshow(STACK, extent=[0.0, AR, 0.005, 1.005], zorder=4)

CX, DY = 0.550, 0.0335
def call(pt, y, head, lines, colour=INK):
    axs.plot([pt[0], CX - 0.055, CX - 0.018], [pt[1], y, y],
             color='#b9bfc7', lw=1.4, zorder=2, solid_capstyle='round')
    axs.plot([pt[0]], [pt[1]], 'o', ms=6, mfc='white', mec='#8f97a1',
             mew=1.4, zorder=6)
    axs.text(CX, y + 0.021, head, fontsize=21, fontweight='bold',
             va='center', color=colour)
    for i, t in enumerate(lines):
        axs.text(CX, y - 0.013 - i * DY, t, fontsize=17, color=colour,
                 va='center')

call((0.375, 0.930), 0.950, "TRANSMIT ARRAY", [
     "a 71 x 47 mm board on a 91 x 67 mm aluminium plate",
     "two patches side by side, so it measures left-and-right",
     "the metal carries the ground 25 mm past every patch"])
call((0.372, 0.660), 0.755, "THE RADIO, AND THE FRONT END", [
     "a USRP B210 in a printed cradle behind the spine",
     "the front-end board plugs onto its four sockets, so no",
     "coaxial cable runs between the radio and the amplifiers"])
call((0.455, 0.500), 0.570, "ISOLATION FIN", [
     "200 mm across, with 100 mm returns folded forward",
     "worth 16.5 dB, and it blocks nothing the radar looks at",
     "the leak goes round the sides, so folding beats widening"], RED)
call((0.300, 0.330), 0.380, "THE SPINE", [
     "two printed halves plus two 155 mm extenders, bolted",
     "through a lap on a 10 mm hole pitch, so the separation",
     "can be set anywhere from 370 to 430 mm"])
call((0.345, 0.108), 0.170, "RECEIVE ARRAY", [
     "a 47 x 71 mm board on a 67 x 91 mm aluminium plate",
     "two patches one above the other: it measures up-and-down",
     "so the pair together gives height as well as bearing"])

TOPY, BOTY = 0.914, 0.112          # the two array centres, as drawn
axs.annotate('', xy=(-0.075, TOPY), xytext=(-0.075, BOTY),
             arrowprops=dict(arrowstyle='<->', lw=2.6, color=BLUE))
for yy in (TOPY, BOTY):
    axs.plot([-0.098, -0.052], [yy, yy], color=BLUE, lw=2.0)
axs.text(-0.116, (TOPY + BOTY) / 2, "400 mm", color=BLUE, fontsize=26,
         fontweight='bold', rotation=90, ha='center', va='center')
axs.text(-0.040, (TOPY + BOTY) / 2, "centre to centre, set by the leak "
         "budget: 4.1 dB in hand", color=BLUE, fontsize=15, rotation=90,
         ha='center', va='center')

# ------------------------------------------------- the live boards, in order
axb = panel([0.020, 0.246, 0.960, 0.200], (0, 10), (0.15, 3.45))
SX, SY = 0.960 * W * DPI / 10.0, 0.200 * H * DPI / 3.30
axb.text(0.0, 3.42, "THE THREE BOARDS THAT ARE BUILT, AND HOW THE SIGNAL "
         "RUNS ROUND THEM", fontsize=24, fontweight='bold', va='top')

TOP, HGT = 2.52, 1.52
def board(key, x, title, sub):
    w = place(axb, key, x, TOP, HGT, SX, SY)
    axb.text(x + w / 2, TOP - HGT - 0.14, title, ha='center', fontsize=18,
             fontweight='bold', va='top')
    for i, t in enumerate(sub):
        axb.text(x + w / 2, TOP - HGT - 0.42 - i * 0.26, t, ha='center',
                 fontsize=15, color='#444', va='top')
    return w

wf = board('fe', 0.60, "FRONT-END BOARD",
           ["100 x 100 mm, 4 layers", "both chains on one laminate"])
wt = board('tx', 4.15, "TRANSMIT ARRAY",
           ["71.3 x 47.2 mm,  2 patches", "right-hand spin"])
wr = board('rx', 8.25, "RECEIVE ARRAY",
           ["47.2 x 71.4 mm", "2 patches,  left-hand spin"])

axb.annotate('', xy=(4.05, 1.75), xytext=(0.60 + wf + 0.10, 1.75),
             arrowprops=dict(arrowstyle='-|>', lw=2.8, color=BLUE))
MID = (4.05 + 0.70 + wf) / 2
axb.text(MID, 2.02, "0.76 watts a chain,", ha='center', fontsize=16, color=BLUE)
axb.text(MID, 1.86, "out to the sky", ha='center', fontsize=16, color=BLUE)
axb.annotate('', xy=(0.60 + wf / 2, 2.62), xytext=(8.25 + wr / 2, 2.62),
             arrowprops=dict(arrowstyle='-|>', lw=2.8, color=BLUE,
                             connectionstyle='arc3,rad=0.055'))
axb.text(5.20, 2.94, "the echo comes back spun the other way",
         ha='center', fontsize=16, color=BLUE)

# ------------------------------------------- superseded pair, and the reasons
axt = panel([0.020, 0.010, 0.960, 0.228], (0, 10), (0, 3))
SY2 = 0.228 * H * DPI / 3.0
axt.text(0.30, 2.97, "ONE BOARD REPLACED THESE TWO", fontsize=20,
         fontweight='bold', va='top')
axt.text(0.30, 2.72, "a transmit amplifier and a receive amplifier, on "
         "two separate laminates", fontsize=15, color=GREY, va='top')
w4 = place(axt, 'pa', 0.30, 2.52, 0.72, SX, SY2)
place(axt, 'lna', 0.30 + w4 + 0.28, 2.52, 0.72, SX, SY2)
for i, t in enumerate(["half the connectors, four fewer cables, one power",
                       "feed and one enclosure -- and the two chains still",
                       "sit 64 dB apart across the board, which is quieter",
                       "than the air path between the two antennas."]):
    axt.text(0.30, 1.66 - i * 0.235, t, fontsize=16, va='top')

axt.add_patch(FancyBboxPatch((0.30, 0.06), 4.30, 0.56,
                             boxstyle='round,pad=0.05', fc='#eef2f8',
                             ec='#9fb3cc', lw=1.4))
axt.text(2.45, 0.53, "WHAT IT DELIVERS", ha='center', fontsize=19,
         fontweight='bold', va='top')
for i, t in enumerate(["a small drone at roughly 900 m, placed to 1 m in range",
                       "all 360 degrees by turning, +/-27 degrees of 3D inside"]):
    axt.text(2.45, 0.32 - i * 0.19, t, ha='center', fontsize=15, va='top')

axt.text(5.35, 2.97, "WHY EACH PIECE IS THERE", fontsize=20,
         fontweight='bold', va='top')
ROWS = [("Two patches, spaced apart",
         "the echo reaches one before the other, and that sliver of time"),
        ("A pair across, and a pair up and down",
         "one measures bearing, the other height; together that is 3D"),
        ("A mast that turns",
         "it carries the 27 degree window round the full circle"),
        ("Circular polarisation",
         "a drone flips the spin, rain and ground bounce do not"),
        ("Aluminium under each antenna board",
         "a patch radiates against metal: bare board 7.6, metal 2.7"),
        ("A fin between the arrays",
         "the transmitter is fifty thousand million times the faintest echo")]
for i, (h, sub) in enumerate(ROWS):
    y = 2.58 - i * 0.415
    axt.text(5.35, y, "\u2022", fontsize=17, va='top', color=BLUE)
    axt.text(5.53, y, h, fontsize=17, fontweight='bold', va='top')
    axt.text(5.53, y - 0.195, sub, fontsize=15, color='#333', va='top')

plt.savefig('SYSTEM.png', dpi=DPI, facecolor='white')
print("  SYSTEM.png  %.0f x %.0f px" % (W * DPI, H * DPI))

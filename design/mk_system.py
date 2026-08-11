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
RC = [0.020, 0.437, 0.960, 0.528]
axs = panel(RC, (0, 1.136), (0, 1.02), equal=True)
AR = STACK.shape[1] / STACK.shape[0]
axs.imshow(STACK, extent=[0.0, AR, 0.005, 1.005], zorder=4)

CX, DY = 0.545, 0.0335
def call(ax_pt, y, head, lines, colour=INK):
    axs.plot([ax_pt[0], CX - 0.055, CX - 0.018], [ax_pt[1], y, y],
             color='#b9bfc7', lw=1.4, zorder=2, solid_capstyle='round')
    axs.plot([ax_pt[0]], [ax_pt[1]], 'o', ms=6, mfc='white',
             mec='#8f97a1', mew=1.4, zorder=6)
    axs.text(CX, y + 0.021, head, fontsize=21, fontweight='bold',
             va='center', color=colour)
    for i, t in enumerate(lines):
        axs.text(CX, y - 0.013 - i * DY, t, fontsize=17, color=colour,
                 va='center')

call((0.375, 0.930), 0.955, "TRANSMIT ARRAY", [
     "a 71 x 47 mm board on a 91 x 67 mm aluminium plate",
     "two patches side by side, so it measures left-and-right",
     "the metal carries the ground 25 mm past every patch"])
call((0.372, 0.660), 0.760, "THE RADIO, AND THE FRONT END", [
     "a USRP B210 in a printed cradle behind the spine",
     "the front-end board plugs straight onto its four sockets",
     "so no coaxial cable runs between radio and amplifier"])
call((0.455, 0.500), 0.575, "ISOLATION FIN", [
     "200 mm across, with 100 mm returns folded forward",
     "worth 16.5 dB, and it blocks nothing the radar looks at",
     "the leak goes round the sides, so folding beats widening"], RED)
call((0.300, 0.330), 0.385, "THE SPINE", [
     "two printed halves plus two 155 mm extenders",
     "bolted through a lap on a 10 mm hole pitch, so the",
     "separation is adjustable from 370 to 430 mm"])
call((0.360, 0.080), 0.175, "RECEIVE ARRAY", [
     "a 47 x 71 mm board on a 67 x 91 mm aluminium plate",
     "two patches one above the other: it measures up-and-down",
     "together the pair of arrays gives height as well as bearing"])

axs.annotate('', xy=(0.075, 0.945), xytext=(0.075, 0.075),
             arrowprops=dict(arrowstyle='<->', lw=2.6, color=BLUE))
axs.text(0.093, 0.560, "400 mm", color=BLUE, fontsize=27, fontweight='bold',
         rotation=90, ha='center', va='center')
for i, t in enumerate(["chosen from the leak budget,",
                       "not from taste: it leaves", "4.1 dB in hand"]):
    axs.text(0.148, 0.560 - (i - 1) * 0.030, t, color=BLUE, fontsize=16,
             rotation=90, ha='center', va='center')

# ------------------------------------------------- the live boards, in order
axb = panel([0.020, 0.243, 0.960, 0.182], (0, 10), (0, 3))
SX, SY = 0.960 * W * DPI / 10.0, 0.182 * H * DPI / 3.0
axb.text(0.0, 2.93, "THE THREE BOARDS THAT ARE BUILT, AND HOW THE SIGNAL "
         "RUNS ROUND THEM", fontsize=24, fontweight='bold', va='top')


def caption(x, w, ytop, hgt, title, sub, note=None):
    axb.text(x + w / 2, ytop - hgt - 0.10, title, ha='center', fontsize=18,
             fontweight='bold', va='top')
    axb.text(x + w / 2, ytop - hgt - 0.34, sub, ha='center', fontsize=15,
             color='#444', va='top')
    if note:
        axb.text(x + w / 2, ytop - hgt - 0.56, note, ha='center', fontsize=14,
                 color=GREY, style='italic', va='top')


TOP, HGT = 2.62, 1.42
w = place(axb, 'fe', 0.30, TOP, HGT, SX, SY)
caption(0.30, w, TOP, HGT, "FRONT-END BOARD",
        "100 x 100 mm, 4 layers -- both chains on one laminate",
        "it replaced the two boards below and plugs onto the radio")
w2 = place(axb, 'tx', 4.30, TOP, HGT * 0.80, SX, SY)
caption(4.30, w2, TOP, HGT * 0.80, "TRANSMIT ARRAY",
        "71.3 x 47.2 mm,  2 patches,  right-hand spin")
w3 = place(axb, 'rx', 7.55, TOP, HGT, SX, SY)
caption(7.55, w3, TOP, HGT, "RECEIVE ARRAY",
        "47.2 x 71.4 mm,  2 patches,  left-hand spin")

axb.annotate('', xy=(4.20, 2.30), xytext=(0.30 + w + 0.10, 2.30),
             arrowprops=dict(arrowstyle='-|>', lw=2.6, color=BLUE))
axb.text((4.30 + 0.30 + w) / 2, 2.40, "0.76 watts a chain, out to the sky",
         ha='center', fontsize=16, color=BLUE)
axb.annotate('', xy=(4.30 + w2 + 0.12, 1.55), xytext=(7.45, 1.55),
             arrowprops=dict(arrowstyle='-|>', lw=2.6, color=BLUE))
axb.text((7.45 + 4.30 + w2) / 2, 1.65, "the echo comes back spun the other way",
         ha='center', fontsize=16, color=BLUE)

# ------------------------------------------------- superseded pair, and why
axt = panel([0.020, 0.010, 0.960, 0.222], (0, 10), (0, 3))
TX2, HG2 = 2.30, 0.80
w4 = place(axt, 'pa', 0.30, TX2, HG2, SX, 0.222 * H * DPI / 3.0)
w5 = place(axt, 'lna', 0.30 + w4 + 0.30, TX2, HG2, SX, 0.222 * H * DPI / 3.0)
axt.text(0.30, TX2 + 0.42, "ONE BOARD REPLACED THESE TWO", fontsize=20,
         fontweight='bold', va='top')
axt.text(0.30, TX2 + 0.15, "a transmit amplifier and a receive amplifier, "
         "on separate laminates", fontsize=15, color=GREY, va='top')
for i, t in enumerate(["half the connectors, four fewer cables, one power",
                       "feed and one enclosure -- and the two chains still",
                       "sit 64 dB apart across the board, which is quieter",
                       "than the air path between the two antennas."]):
    axt.text(0.30, TX2 - HG2 - 0.22 - i * 0.235, t, fontsize=16, va='top')

axt.text(5.45, TX2 + 0.42, "WHY EACH PIECE IS THERE", fontsize=20,
         fontweight='bold', va='top')
ROWS = [("Two patches, spaced apart",
         "the echo reaches one before the other, and that sliver of time is the angle"),
        ("A pair across, and a pair up and down",
         "one measures bearing, the other height; together that is 3D"),
        ("A mast that turns",
         "it carries the 27 degree window round the full circle -- the 3D stays electronic"),
        ("Circular polarisation",
         "a drone flips the spin, rain and ground bounce do not: 14 dB of clutter gone"),
        ("Aluminium under each antenna board",
         "a patch radiates against metal: 6 mm of bare board measures 7.6, 25 mm of metal 2.7"),
        ("A fin between the arrays",
         "the transmitter is fifty thousand million times louder than the faintest echo")]
for i, (h, s) in enumerate(ROWS):
    y = TX2 + 0.03 - i * 0.375
    axt.text(5.45, y, "•", fontsize=17, va='top', color=BLUE)
    axt.text(5.62, y, h, fontsize=17, fontweight='bold', va='top')
    axt.text(5.62, y - 0.185, s, fontsize=15, color='#333', va='top')

axt.add_patch(FancyBboxPatch((0.30, -0.02), 4.55, 0.62, boxstyle='round,pad=0.05',
                             fc='#eef2f8', ec='#9fb3cc', lw=1.4))
axt.text(2.58, 0.46, "WHAT IT DELIVERS", ha='center', fontsize=19,
         fontweight='bold', va='top')
for i, t in enumerate(["a small drone at roughly 900 m, told apart to 1 m in range",
                       "all 360 degrees by turning, and +/-27 degrees of 3D inside that"]):
    axt.text(2.58, 0.22 - i * 0.20, t, ha='center', fontsize=15, va='top')

plt.savefig('SYSTEM.png', dpi=DPI, facecolor='white')
print("  SYSTEM.png  %.0f x %.0f px" % (W * DPI, H * DPI))

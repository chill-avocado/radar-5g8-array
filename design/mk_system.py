
import json, matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.image as mpimg
from matplotlib.patches import FancyBboxPatch, Rectangle

D = 'design/'
tx = json.load(open(D+'board_transmit.json'))
rx = json.load(open(D+'board_receive.json'))
fe = json.load(open(D+'fe_board.json'))
pa = json.load(open(D+'pa_board.json'))
ln = json.load(open(D+'lna_board.json'))
IM = {k: mpimg.imread(D+'brd_%s.png' % k) for k in ('tx','rx','fe','pa','lna')}
SEP = 400.0

fig = plt.figure(figsize=(23, 13.5))
gs = fig.add_gridspec(1, 2, width_ratios=[0.80, 1.45], wspace=0.03)

# ============================================ LEFT: the stack, to scale
ax = fig.add_subplot(gs[0, 0]); ax.set_aspect('equal'); ax.axis('off')
ax.set_title("HOW IT STACKS      side elevation, to scale",
             fontsize=14, fontweight='bold', pad=12)
LX, DY = 120.0, 19.0
def lab(y_part, y_txt, head, lines, colour='#222', anchor=52.0):
    ax.plot([anchor, LX-40, LX-6], [y_part, y_txt, y_txt],
            color='#aaa', lw=.9, zorder=1)
    ax.text(LX, y_txt+11, head, fontsize=10.6, fontweight='bold',
            va='center', color=colour)
    for i, t in enumerate(lines):
        ax.text(LX, y_txt-7-i*DY, t, fontsize=9.2, color=colour, va='center')

# the radio sits BEHIND the spine, spanning the fin -- drawn dashed, in place
RY = -SEP/2
ax.add_patch(Rectangle((-48.5, RY-77.5), 97, 155, fc='#5b6673', ec='#333',
                       lw=1, ls=(0, (5, 3)), zorder=2, alpha=.55))
ax.imshow(IM['fe'], extent=[-46, 46, RY-40, RY+52], zorder=2, alpha=.92)

for cy, (pw, ph), b_, key, head, ytxt in (
        (0.0, (91, 67), tx, 'tx', 'TRANSMIT ARRAY', 30),
        (-SEP, (67, 91), rx, 'rx', 'RECEIVE ARRAY', -404)):
    ax.add_patch(Rectangle((-pw/2, cy-ph/2), pw, ph, fc='#b9c0c7', ec='k',
                           lw=1.3, zorder=5))
    bw, bh = b_['outline']
    ox = -pw/2 + (10.0 if key == 'tx' else 0.0)
    oy = cy - ph/2 + (0.0 if key == 'tx' else 10.0)
    ax.imshow(IM[key], extent=[ox, ox+bw, oy, oy+bh], zorder=6)
    lab(cy, ytxt, head, [
        "board %.1f x %.1f mm, 2-layer PTFE" % (bw, bh),
        "on a %g x %g x 1.5 mm aluminium plate," % (pw, ph),
        "which carries the ground 25 mm past",
        "every patch edge -- and is not optional",
        "two patches: measures " + ("LEFT-RIGHT" if key=='tx' else "UP-DOWN"),
    ], anchor=pw/2)

ax.add_patch(Rectangle((-12.5, -SEP+52), 25, SEP-104, fc='#4a6fa5', ec='k',
                       lw=1, zorder=4))
lab(-108, -100, "SPINE", ["two printed halves and two",
                          "155 mm extenders, lapped and bolted"], anchor=12.5)
ax.add_patch(Rectangle((-100, RY-4), 200, 8, fc='#8d99ae', ec='k',
                       lw=1.1, zorder=7))
lab(RY, RY+12, "ISOLATION FIN",
    ["200 mm across, 100 mm forward, with",
     "100 mm returns folded at the sides.",
     "The leak goes round the edges, not over",
     "the tip, so folding beats widening."],
    colour='#b03030', anchor=100)
lab(RY-77, RY-118, "USRP B210, behind the spine",
    ["in a printed cradle, with the front-end",
     "board plugged onto its four sockets"], anchor=48.5)

ax.annotate('', xy=(-128, 0), xytext=(-128, -SEP),
            arrowprops=dict(arrowstyle='<->', lw=1.8, color='#204090'))
ax.text(-138, -SEP/2+22, "400 mm", color='#204090', ha='right',
        fontsize=13, fontweight='bold')
for i, t in enumerate(["from the leak budget,", "not from taste:",
                       "4.1 dB in hand"]):
    ax.text(-138, -SEP/2+1-i*17, t, color='#204090', ha='right', fontsize=9)
ax.set_xlim(-300, 470); ax.set_ylim(-SEP-135, 95)

# ============================================ RIGHT: the boards themselves
ax2 = fig.add_subplot(gs[0, 1]); ax2.axis('off')
ax2.set_xlim(0, 10); ax2.set_ylim(-1.15, 11.9)
ax2.set_title("THE FOUR BOARDS, AND HOW THE SIGNAL RUNS THROUGH THEM",
              fontsize=14, fontweight='bold', pad=12)

def show(key, x, y, w, title, sub, note='', fs=10.4):
    """y is the BOTTOM of the caption; the board sits above it."""
    im = IM[key]
    h = w * im.shape[0] / im.shape[1]
    cap = 0.46 if note else 0.34
    by = y + cap
    ax2.imshow(im, extent=[x, x+w, by, by+h], zorder=3)
    ax2.add_patch(Rectangle((x, by), w, h, fc='none', ec='k', lw=1.1, zorder=4))
    ax2.text(x+w/2, y+cap-0.14, title, ha='center', fontsize=fs,
             fontweight='bold', va='top')
    ax2.text(x+w/2, y+cap-0.33, sub, ha='center', fontsize=8.5, color='#333',
             va='top')
    if note:
        ax2.text(x+w/2, y+cap-0.50, note, ha='center', fontsize=8.2,
                 color='#777', style='italic', va='top')
    return h + cap

def ar(x0, y0, x1, y1, t='', off=0.16):
    ax2.annotate('', xy=(x1, y1), xytext=(x0, y0),
                 arrowprops=dict(arrowstyle='-|>', lw=2.0, color='#204090'))
    if t:
        ax2.text((x0+x1)/2, (y0+y1)/2+off, t, ha='center', fontsize=8.6,
                 color='#204090')

show('fe', 0.30, 8.05, 3.00, "FRONT-END BOARD",
     "100 x 100 mm, 4 layer -- both chains on one laminate",
     "plugs straight onto the radio's four sockets")
show('tx', 6.55, 9.45, 2.95, "TRANSMIT ARRAY",
     "71.3 x 47.2 mm   2 patches, RHCP")
show('rx', 8.25, 5.55, 1.45, "RECEIVE ARRAY",
     "47.2 x 71.4 mm   2 patches, LHCP")
ar(3.40, 10.30, 6.45, 10.55, "0.76 W a chain")
ax2.plot([8.20, 4.08, 4.08], [5.22, 5.22, 8.62], color='#204090',
         lw=2.0, solid_joinstyle='round', zorder=3)
ax2.annotate('', xy=(3.34, 8.62), xytext=(4.02, 8.62),
             arrowprops=dict(arrowstyle='-|>', lw=2.0, color='#204090'))
ax2.text(6.30, 5.38, "the echo, spun the other way", fontsize=8.7,
         color='#204090', ha='center')

show('pa', 0.35, 5.30, 1.60, "TRANSMIT AMPLIFIER", "100 x 84 mm",
     "superseded", fs=9.3)
show('lna', 2.30, 5.30, 1.55, "RECEIVE AMPLIFIER", "92 x 68 mm",
     "superseded", fs=9.3)

ax2.text(4.45, 7.14, "One board replaced these two", fontsize=10,
         fontweight='bold', va='top')
for i, t in enumerate([
        "half the connectors, four fewer cables,",
        "one power feed, one enclosure -- and the",
        "two chains still 64 dB apart across the",
        "laminate, quieter than the air path",
        "between the two antennas."]):
    ax2.text(4.45, 6.86 - i*0.222, t, fontsize=9.0, color='#333', va='top')

ax2.text(0.35, 4.30, "WHY EACH PIECE IS THERE", fontsize=11.5,
         fontweight='bold')
rows = [
 ("Two patches, spaced apart",
  "the echo reaches one before the other, and that sliver of time is the angle"),
 ("Transmit pair across, receive pair up and down",
  "one measures left-right, the other up-down; together that is 3D"),
 ("Spinning mast",
  "carries the 27 degree window round the full circle -- the 3D stays electronic"),
 ("Circular polarisation",
  "a drone flips the spin, rain and ground bounce do not: 14 dB of clutter gone"),
 ("Aluminium plate under each antenna board",
  "a patch radiates against metal.  6 mm of bare board measures 7.6; 25 mm of metal, 2.7"),
 ("Fin between the arrays",
  "the transmitter is fifty thousand million times louder than the faintest echo"),
]
y = 3.92
for a, b in rows:
    ax2.text(0.40, y, "•   " + a, fontsize=9.8, fontweight='bold', va='top')
    ax2.text(0.85, y-0.285, b, fontsize=9.0, color='#333', va='top')
    y -= 0.585
ax2.add_patch(FancyBboxPatch((0.35, -1.02), 9.4, 1.04,
                             boxstyle="round,pad=0.06", fc='#eef2f6',
                             ec='#8a9099'))
ax2.text(5.05, -0.19, "WHAT IT DELIVERS", ha='center', fontsize=11,
         fontweight='bold')
ax2.text(5.05, -0.51, "small drone at roughly 900 m        "
         "1 m range resolution        licence-exempt 5.8 GHz",
         ha='center', fontsize=9.2)
ax2.text(5.05, -0.80, "360 degrees by rotation, and +/-27 degrees of "
         "electronic 3D inside that", ha='center', fontsize=9.2)
fig.suptitle("5.8 GHz DRONE RADAR  --  HOW EVERY PIECE FITS TOGETHER",
             fontsize=17, fontweight='bold', y=0.995)
plt.savefig('SYSTEM.png', dpi=95, bbox_inches='tight')
print("  SYSTEM.png rebuilt with the real boards")

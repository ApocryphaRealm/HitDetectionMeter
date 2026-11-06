"""Hit Direction Indicator - the Nordic marker art, drawn from scratch with Pillow and numpy (2026-11-06).

The owner asked for a hit marker that looks Nordic - carved stone, iron, runic knotwork, muted bone and blood red, the
dark carved cold look of Norden UI Black, not a modern shooter arrow. This draws ONE texture: a curved slab of dark
carved stone held in an iron rim, a blood-red channel cut into it with a two-strand knotwork braid running through
it, bone rune ticks at both ends, and a Tiwaz rune (the arrow rune) at the middle, pointing out from the centre of the
screen - the way the hit came from.

The texture is used only when Detection Meter (Nexus 63057) is NOT installed. With it, the mod draws Detection Meter's
own Meter_Frame.png / Meter_Hostile.png, whichever art pack wins, so the two read as one HUD.

Like Detection Meter's art, the texture's TOP edge faces away from the screen centre and the mod draws it centred on
the ring, turned to the hit's bearing. The slab is bent to a ring of R_RING pixels (in 1080p units, at 2x here), the
default ring the hit markers sit on (Detection Meter's RadiusX 305 + the default 40 px offset).

Output: dist/SKSE/Plugins/HitDirectionIndicator/HitMarker_Nordic.png (320 x 88, drawn at 160 x 44 on a 1080p screen),
and with --preview, two 1920 x 1080 previews in tools/preview/.
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(REPO, "dist", "SKSE", "Plugins", "HitDirectionIndicator")
PREVIEW_DIR = os.path.join(REPO, "tools", "preview")

SCALE = 2                 # texture pixels per 1080p pixel
W, H = 160 * SCALE, 44 * SCALE
R_RING = 345.0 * SCALE    # the ring the slab is bent to
HALF_SPAN = math.degrees((W * 0.46) / R_RING)   # half the arc the slab covers
SS = 4                    # supersampling

STONE_DARK = np.array([0.105, 0.100, 0.095])
STONE_LIGHT = np.array([0.215, 0.205, 0.190])
IRON = np.array([0.36, 0.37, 0.39])
IRON_HI = np.array([0.66, 0.67, 0.68])
BONE = np.array([0.85, 0.80, 0.69])
BLOOD = np.array([0.56, 0.09, 0.06])
BLOOD_HI = np.array([0.80, 0.20, 0.12])


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def value_noise(shape, cell, seed):
    rng = np.random.default_rng(seed)
    gh, gw = shape[0] // cell + 2, shape[1] // cell + 2
    g = rng.random((gh, gw))
    img = Image.fromarray((g * 255).astype(np.uint8)).resize((gw * cell, gh * cell), Image.BICUBIC)
    return np.asarray(img, dtype=np.float64)[: shape[0], : shape[1]] / 255.0


def draw_marker():
    WS, HS = W * SS, H * SS
    y, x = np.mgrid[0:HS, 0:WS].astype(np.float64)
    x = (x + 0.5) / SS
    y = (y + 0.5) / SS
    # the ring's centre lies far below the texture, so the slab bends round it: its top edge is its outer edge
    cx, cy = W / 2.0, H * 0.40 + R_RING
    dx, dy = x - cx, cy - y
    r = np.hypot(dx, dy)
    theta = np.degrees(np.arctan2(dx, dy))     # 0 at the middle, + to the right
    u = theta / HALF_SPAN                      # -1 .. 1 along the slab
    v = (r - R_RING) / (H * 0.5)               # radial, in half-texture-heights: + outward
    L = HALF_SPAN * math.pi / 180.0 * R_RING   # pixels per unit of u
    P = H * 0.5                                # pixels per unit of v

    # the outline: an even carved band whose ends are cut to points (a long wedge, not a lens)
    taper = np.clip(1.0 - np.abs(u) ** 5.0, 0.0, None) ** 0.45
    half_thick = np.where(np.abs(u) < 1.0, 0.58 * taper, 0.0)
    inside = smoothstep(half_thick + 0.03, half_thick - 0.03, np.abs(v)) * (np.abs(u) < 1.0)
    edge_d = half_thick - np.abs(v)            # distance in from the outline (v units)

    # stone: two octaves of value noise, lit from above (the outer half a shade lighter: a convex carved face)
    n1 = value_noise((HS, WS), 10 * SS, 7)
    n2 = value_noise((HS, WS), 3 * SS, 11)
    grain = 0.6 * n1 + 0.4 * n2
    stone = STONE_DARK[None, None, :] + (STONE_LIGHT - STONE_DARK)[None, None, :] * grain[..., None]
    face = np.clip(0.95 + 0.45 * v, 0.70, 1.25)
    rgb = stone * face[..., None]

    # iron rim round the outline, a bright bevel on its outer (top) side
    rim = smoothstep(0.15, 0.09, edge_d) * inside
    rim_lit = np.clip(0.45 + 1.1 * v, 0.0, 1.0)
    iron = IRON[None, None, :] + (IRON_HI - IRON)[None, None, :] * rim_lit[..., None]
    rgb = rgb * (1.0 - rim[..., None]) + iron * rim[..., None]
    # rivets in the rim, top and bottom
    for ru in np.linspace(-0.70, 0.70, 6):
        ht = 0.58 * max(0.0, 1.0 - abs(ru) ** 5.0) ** 0.45
        for side in (-1.0, 1.0):
            rv = side * (ht - 0.085)
            d = np.hypot((u - ru) * L, (v - rv) * P)
            dot = smoothstep(2.0, 1.2, d) * inside
            rgb = rgb * (1.0 - dot[..., None]) + (IRON_HI * 1.05)[None, None, :] * dot[..., None]

    # the blood channel: a groove cut along the middle of the band, muted dark red
    chan_half = 0.27 * np.clip(1.0 - np.abs(u / 0.80) ** 6.0, 0.0, None) ** 0.5
    chan = smoothstep(chan_half + 0.025, chan_half - 0.025, np.abs(v))
    groove_shadow = smoothstep(chan_half + 0.09, chan_half, np.abs(v)) * (1.0 - chan) * (chan_half > 0)
    rgb = rgb * (1.0 - 0.5 * groove_shadow[..., None])
    blood = BLOOD[None, None, :] * (0.70 + 0.40 * n2[..., None])
    # knotwork: two bone strands interlaced along the channel; at each crossing one passes over the other, the
    # strand underneath is cut back on both sides of it, and the one on top changes from crossing to crossing
    freq = 7.0
    s1 = 0.62 * chan_half * np.sin(u * math.pi * freq)
    s2 = -s1
    w = 0.055
    d1, d2 = np.abs(v - s1), np.abs(v - s2)
    on1 = smoothstep(w, w * 0.5, d1)
    on2 = smoothstep(w, w * 0.5, d2)
    k = np.floor(u * freq + 0.5)               # which crossing this column is nearest
    top1 = (k % 2) == 0
    gap1 = smoothstep(w * 2.2, w * 1.3, d1) * top1      # strand 2 is cut where strand 1 lies over it
    gap2 = smoothstep(w * 2.2, w * 1.3, d2) * (~top1)
    on2 = on2 * (1.0 - gap1)
    on1 = on1 * (1.0 - gap2)
    strands = np.clip(on1 + on2, 0.0, 1.0) * (np.abs(u) < 0.70)
    bone = BONE * 0.82
    channel_rgb = blood * (1.0 - strands[..., None]) + bone[None, None, :] * strands[..., None]
    rgb = rgb * (1.0 - chan[..., None]) + channel_rgb * chan[..., None]

    # bone rune ticks near each end, three short incised strokes like a carved count
    ticks = np.zeros_like(u)
    for end in (-1.0, 1.0):
        for kk in range(3):
            tu = end * (0.76 - 0.035 * kk)
            stroke = smoothstep(1.2, 0.5, np.abs(u - tu) * L) * smoothstep(0.24, 0.17, np.abs(v))
            ticks = np.maximum(ticks, stroke)
    ticks = ticks * inside * (1.0 - rim)
    rgb = rgb * (1.0 - ticks[..., None]) + (BONE * 0.9)[None, None, :] * ticks[..., None]

    # an iron boss at the middle for the rune to sit on
    boss_d = np.hypot(u * L, v * P)
    boss = smoothstep(15.5, 14.0, boss_d)
    boss_rim = smoothstep(15.5, 14.0, boss_d) * smoothstep(11.5, 13.0, boss_d)
    boss_rgb = STONE_DARK * 0.8
    rgb = rgb * (1.0 - boss[..., None]) + boss_rgb[None, None, :] * boss[..., None]
    rgb = rgb * (1.0 - boss_rim[..., None]) + (IRON_HI * 0.9)[None, None, :] * boss_rim[..., None]

    alpha = inside.copy()
    shadow = smoothstep(half_thick + 0.22, half_thick, np.abs(v)) * (np.abs(u) < 1.02) * 0.5   # reads on snow
    alpha = np.maximum(alpha, shadow)
    rgb = rgb * inside[..., None]

    img = np.dstack([np.clip(rgb, 0, 1), np.clip(alpha, 0, 1)]) * 255.0
    im = Image.fromarray(img.astype(np.uint8), "RGBA").resize((W, H), Image.LANCZOS)

    # the Tiwaz rune on the boss: an arrow-rune pointing out from the screen centre, the way the hit came from
    rune = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(rune)
    mx, my = W / 2.0, H * 0.40
    top, bot, arm = my - 9.0 * SCALE, my + 9.0 * SCALE, 5.5 * SCALE
    for colour, width in (((10, 9, 8, 230), int(4.2 * SCALE)), ((int(BLOOD_HI[0] * 230), int(BLOOD_HI[1] * 230), int(BLOOD_HI[2] * 230), 255), int(2.0 * SCALE))):
        d.line([(mx, top), (mx, bot)], fill=colour, width=width)
        d.line([(mx - arm, top + arm), (mx, top), (mx + arm, top + arm)], fill=colour, width=width, joint="curve")
    rune = rune.filter(ImageFilter.GaussianBlur(0.3 * SCALE))
    im = Image.alpha_composite(im, rune)
    return im


def ring_preview(marker, out_path, screen_bg, bearings, radius, centre_off, draw_size, extra=None):
    """A 1920 x 1080 picture of markers drawn as the mod draws them: centred on the ring at each bearing, the texture's
    top edge facing out, faded by age."""
    bg = screen_bg.copy()
    cx, cy = 960 + centre_off[0], 540 + centre_off[1]
    for bearing, alpha in bearings:
        m = marker.resize((int(draw_size[0]), int(draw_size[1])), Image.LANCZOS)
        if alpha < 1.0:
            a = np.asarray(m, dtype=np.float64)
            a[..., 3] *= alpha
            m = Image.fromarray(a.astype(np.uint8), "RGBA")
        rot = m.rotate(-bearing, resample=Image.BICUBIC, expand=True)
        px = cx + radius * math.sin(math.radians(bearing))
        py = cy - radius * math.cos(math.radians(bearing))
        bg.alpha_composite(rot, (int(px - rot.width / 2), int(py - rot.height / 2)))
    if extra:
        extra(bg)
    bg.convert("RGB").save(out_path)
    print("preview ->", out_path)


def backdrop():
    """A cold dusk Skyrim-ish backdrop: dark blue-grey sky over snow, so the art is judged against both."""
    h, w = 1080, 1920
    yy = np.linspace(0, 1, h)[:, None]
    sky = np.array([0.20, 0.23, 0.28]) * (1 - yy) + np.array([0.55, 0.57, 0.60]) * yy
    img = np.repeat(sky[:, None, :], w, axis=1).reshape(h, w, 3)
    n = value_noise((h, w), 40, 3)[..., None]
    img = np.clip(img * (0.9 + 0.2 * n), 0, 1)
    return Image.fromarray((img * 255).astype(np.uint8), "RGB").convert("RGBA")


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    im = draw_marker()
    path = os.path.join(OUT_DIR, "HitMarker_Nordic.png")
    im.save(path)
    print("HitMarker_Nordic", im.size, "->", path)

    if "--preview" in sys.argv:
        os.makedirs(PREVIEW_DIR, exist_ok=True)
        bg = backdrop()
        hits = [(-62.0, 1.0), (8.0, 0.75), (128.0, 0.45), (-150.0, 0.25)]
        ring_preview(im, os.path.join(PREVIEW_DIR, "preview-nordic.png"), bg, hits, 345.0, (0, -48), (160, 44))
        # with Detection Meter's art (whichever pack is given): frame then hostile fill, on the ring 40 px outside its own
        dm = [a for a in sys.argv[1:] if a.lower().endswith(".png")]
        if len(dm) == 2:
            frame = Image.open(dm[0]).convert("RGBA")
            fill = Image.open(dm[1]).convert("RGBA")
            both = Image.alpha_composite(frame, fill)

            def detection_marker(canvas):   # one detection marker (sneaking) at -62 degrees, on Detection Meter's own ring
                m = frame.copy()
                rot = m.rotate(62.0, resample=Image.BICUBIC, expand=True)
                px = 960 + 305 * math.sin(math.radians(-62.0))
                py = 540 - 48 - 305 * math.cos(math.radians(-62.0))
                canvas.alpha_composite(rot, (int(px - rot.width / 2), int(py - rot.height / 2)))

            ring_preview(both, os.path.join(PREVIEW_DIR, "preview-detection-meter-art.png"), bg, hits, 345.0, (0, -48), both.size, detection_marker)


if __name__ == "__main__":
    main()

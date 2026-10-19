"""Validate SVG/catalog portability and build the two vector overview sheets.

Python stdlib only. For raster render verification afterwards run
render_concept_pack.cjs with Node + sharp available through NODE_PATH.
"""
from pathlib import Path
import hashlib
import html
import json
import math
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent
PACK = 'fader_control_track_utility_v1'
NS = '{http://www.w3.org/2000/svg}'


def overview(name, title, sections):
    width, y = 1440, 132
    bits = ['<rect width="1440" height="100%" fill="#111720"/>',
            '<path d="M24 104 H1416" stroke="#4B5F7B"/>',
            f'<text x="28" y="53" font-size="30" fill="#DAE5F4">MR Studio — {html.escape(title)}</text>',
            '<text x="28" y="83" font-size="14" fill="#A9BFDC">SVG asset library · concept pack v1.0 · original vectors · static design samples</text>']
    for label, entries, row_h in sections:
        bits.append(f'<text x="28" y="{y+15}" font-size="15" letter-spacing="1.5" fill="#91BBEE">{html.escape(label.upper())}</text>')
        y += 34
        columns = 8
        cw = 174
        for i, a in enumerate(entries):
            x, yy = 24+(i % columns)*cw, y+(i//columns)*row_h
            bits.append(f'<rect x="{x}" y="{yy}" width="162" height="{row_h-12}" rx="7" fill="#192333" stroke="#34465E"/>')
            root = ET.parse(ROOT/a['file']).getroot()
            w, h = map(float, root.get('viewBox').split()[2:])
            max_w, max_h = 136, row_h-62
            scale = min(max_w/w, max_h/h, 1)
            # Standard glyphs show at native 24px, compact at native 16px.
            ax, ay = x+(162-w*scale)/2, yy+12+(max_h-h*scale)/2
            inner = ''.join(ET.tostring(child, encoding='unicode') for child in root if child.tag != NS+'title')
            bits.append(f'<g transform="translate({ax:g} {ay:g}) scale({scale:g})">{inner}</g>')
            label_name = a.get('label', a.get('role', a.get('id', '').split('/')[-1]))
            words = label_name.replace('_', ' ').split()
            lines = ['']
            for word in words:
                if len(lines[-1])+len(word)>22:
                    lines.append('')
                lines[-1] += (' ' if lines[-1] else '')+word
            for j, text in enumerate(lines[:2]):
                bits.append(f'<text x="{x+81}" y="{yy+row_h-38+j*14}" text-anchor="middle" font-size="11" fill="#DAE5F4">{html.escape(text)}</text>')
        y += math.ceil(len(entries)/columns)*row_h+20
    bits.append(f'<text x="28" y="{y+4}" font-size="12" fill="#7188AB">Host supplies input, text, units, accessibility and live measurements. No runtime integration.</text>')
    height = y+30
    data = (f'<svg xmlns="http://www.w3.org/2000/svg" width="1440" height="{height}" viewBox="0 0 1440 {height}">'
            '<g font-family="Segoe UI, sans-serif">'+''.join(bits)+'</g></svg>\n')
    (ROOT/'previews'/name).write_text(data, encoding='utf-8')


def main():
    catalog = json.loads((ROOT/'catalogs'/f'{PACK}.json').read_text(encoding='utf-8'))
    manifest = json.loads((ROOT/'manifest.json').read_text(encoding='utf-8'))
    assets = catalog['assets']
    ids = [a['id'] for a in manifest['assets']]
    assert len(ids) == len(set(ids)), 'Duplicate manifest IDs'
    assert manifest['total_asset_svg'] == len(ids)
    assert [a for a in manifest['assets'] if a.get('pack') == PACK] == assets
    palette = json.loads((ROOT/'tokens/concept_pack.json').read_text())['palette']
    def luminance(color):
        rgb = [int(color[i:i+2], 16)/255 for i in (1, 3, 5)]
        return sum(w*(v/12.92 if v <= .04045 else ((v+.055)/1.055)**2.4)
                   for w, v in zip((.2126, .7152, .0722), rgb))
    for a in assets:
        if a['family'] == 'track_tags':
            bg = palette[a['id'].split('_')[-1]]
            lo, hi = sorted((luminance(bg), luminance(a['text_color'])))
            assert (hi+.05)/(lo+.05) >= 4.5, 'Insufficient tag text contrast: '+a['id']
    allowed = {'svg', 'title', 'g', 'path', 'rect', 'circle', 'ellipse'}
    for a in assets:
        path = (ROOT/a['file']).resolve()
        assert path.is_relative_to(ROOT/'svg'), path
        root = ET.parse(path).getroot()
        assert root.tag == NS+'svg'
        assert list(map(float, root.get('viewBox').split())) == [0, 0, *a['size']]
        for el in root.iter():
            assert el.tag.removeprefix(NS) in allowed, (path, el.tag)
            for k, v in el.attrib.items():
                assert not k.lower().startswith('on') and k not in ('href', 'style', 'class')
                assert not any(x in v.lower() for x in ('url(', 'data:', 'javascript:'))
    for a in catalog['reused']:
        path = (ROOT/a['file']).resolve()
        assert path.is_relative_to(ROOT.parent) and path.is_file(), path
        ET.parse(path)
    # Preservation evidence is optional when validating outside the original workspace.
    baseline = ROOT.parents[2]/'build/validation/svg-concept-pack/baseline.json'
    unchanged = 0
    if baseline.exists():
        for rel, digest in json.loads(baseline.read_text()).items():
            if rel.replace('\\', '/') in ('assets/ui/mr-uds/manifest.json', 'assets/ui/mr-uds/README.md'):
                continue
            p = ROOT.parents[2]/rel
            assert hashlib.sha256(p.read_bytes()).hexdigest() == digest, f'Existing file modified: {rel}'
            unchanged += 1
    select = lambda f: [a for a in assets if f(a)]
    controls = [
        ('01 / Vertical faders · normal / hover / active / focus / automation / disabled', select(lambda a: a['family'] in ('fader_micro', 'fader_precision') and a.get('kind') == 'state_sample'), 262),
        ('02 / Horizontal faders and separate layers', select(lambda a: a['family'] == 'fader_slide' and a.get('kind') == 'state_sample') + select(lambda a: a['family'] == 'fader_micro' and (a['id'].endswith('_track') or a['id'].endswith('_thumb_normal'))), 144),
        ('03 / Meters · sample levels, no measured audio', select(lambda a: a['family'] == 'meters'), 214),
        ('04 / Scales and pan', select(lambda a: a['family'] in ('scales', 'pan')), 210),
        ('05 / Mute, solo, record · off / on', select(lambda a: a['family'] == 'channel_buttons'), 114),
        ('06 / Value fields and toolbar frames · host draws values and glyphs', select(lambda a: a['family'] in ('value_capsules', 'toolbar_buttons', 'transport')), 108),
        ('07 / Track tags and decorative waveform illustrations', select(lambda a: a['family'] in ('track_tags', 'waveforms')), 128),
        ('08 / Dividers and spacing samples', select(lambda a: a['family'] in ('dividers', 'spacers')), 154)]
    icons = [
        ('01 / Tracks · native 24 px', select(lambda a: a['family'] == 'tracks' and a['lod'] == 'standard_24'), 112),
        ('02 / Tracks · simplified compact 16 px', select(lambda a: a['family'] == 'tracks' and a['lod'] == 'compact_16'), 100),
        ('03 / Utilities, routing, browser, markers and plugins · native 24 px', select(lambda a: a['family'] == 'utilities'), 112),
        ('04 / Automation · shape and color states', select(lambda a: a['family'] == 'automation'), 112),
        ('05 / Reused canonical glyphs · original files', catalog['reused'], 112)]
    overview('Fader_Control_Overview.svg', 'Faders & Controls', controls)
    overview('Track_Utility_Overview.svg', 'Tracks & Utilities', icons)
    print(f'PASS: {len(assets)} SVG XML/safety/dimension checks; {len(catalog["reused"])} reuse paths; {unchanged} baseline files unchanged; 2 overview sheets')


if __name__ == '__main__':
    main()

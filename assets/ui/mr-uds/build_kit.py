#!/usr/bin/env python3
"""Generate original SVG design system for MR Studio native plug-ins. Python3 + cairosvg + Pillow."""
from pathlib import Path
import math, html, json, zipfile, io, re
from xml.etree import ElementTree as ET
from PIL import Image, ImageDraw, ImageFont
import cairosvg

ROOT=Path(__file__).resolve().parent
C={
  'canvas':'#111720','window':'#171E28','surface':'#222C3C','raised':'#2D394E','deep':'#192333',
  'stroke':'#A9BFDC','bright':'#DAE5F4','muted':'#7188AB','subtle':'#4B5F7B',
  'accent':'#91BBEE','red':'#E68F88','yellow':'#E2C070','green':'#7AB69E','violet':'#AB92DD'
}
OUTLINE=f'fill="none" stroke="{C["stroke"]}" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"'
FAINT=f'fill="none" stroke="{C["muted"]}" stroke-width="1.1" stroke-linecap="round" stroke-linejoin="round"'
SOFT=f'fill="none" stroke="{C["subtle"]}" stroke-width="1.1" stroke-linecap="round" stroke-linejoin="round"'
BRIGHT=f'fill="none" stroke="{C["bright"]}" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"'

class Art:
    def __init__(self,w=320,h=200): self.w=w; self.h=h; self.buf=[]
    def add(self,x): self.buf.append(x); return self
    def line(self,x1,y1,x2,y2,sty=OUTLINE):return self.add(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" {sty}/>')
    def rect(self,x,y,w,h,r=0,sty=OUTLINE):return self.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" {sty}/>')
    def path(self,d,sty=OUTLINE):return self.add(f'<path d="{d}" {sty}/>')
    def circle(self,x,y,r,sty=OUTLINE):return self.add(f'<circle cx="{x}" cy="{y}" r="{r}" {sty}/>')
    def ellipse(self,x,y,rx,ry,sty=OUTLINE):return self.add(f'<ellipse cx="{x}" cy="{y}" rx="{rx}" ry="{ry}" {sty}/>')
    def dot(self,x,y,r=2,color=None):return self.circle(x,y,r,f'fill="{color or C["stroke"]}" stroke="none"')
    def text(self,x,y,s,size=10,color=None,anchor='middle',spacing=None,weight='400'):
        ls=f' letter-spacing="{spacing}"' if spacing is not None else ''
        return self.add(f'<text x="{x}" y="{y}" font-family="Inter,Segoe UI,sans-serif" font-size="{size}" fill="{color or C["muted"]}" text-anchor="{anchor}" font-weight="{weight}"{ls}>{html.escape(str(s))}</text>')
    def poly(self,pts,sty=OUTLINE): return self.add('<polyline points="'+' '.join(f'{x},{y}' for x,y in pts)+f'" {sty}/>')
    def svg(self): return f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w}" height="{self.h}" viewBox="0 0 {self.w} {self.h}">\n<title>MR Studio GUI element</title>\n'+'\n'.join(self.buf)+'\n</svg>\n'

ASSETS={}

def save(category,name,a):
    d=ROOT/'svg'/category;d.mkdir(parents=True,exist_ok=True)
    fn=d/f'{name}.svg';fn.write_text(a.svg(),encoding='utf-8')
    ET.parse(fn)
    ASSETS[f'{category}/{name}']={'path':str(fn.relative_to(ROOT)),'width':a.w,'height':a.h,'fragment':'\n'.join(a.buf)}

def unit_knob(a,cx,cy,r=29,ang=34):
    a.circle(cx,cy,r,FAINT)
    # active highlighted sweep, without gradients
    a.path(f'M {cx-r*.76:.2f} {cy+r*.65:.2f} A {r} {r} 0 1 1 {cx+r*.76:.2f} {cy+r*.65:.2f}',f'fill="none" stroke="{C["accent"]}" stroke-width="2.4" stroke-linecap="round"')
    a.circle(cx,cy,r-8,SOFT)
    theta=math.radians(ang-90)
    a.line(cx,cy,cx+math.cos(theta)*(r-10),cy+math.sin(theta)*(r-10),BRIGHT)
    a.dot(cx,cy,1.9,C['bright'])

def pedal_body(a, label,mode):
    a.path('M 105 40 L 211 40 L 231 155 Q 232 161 224 164 L 96 164 Q 89 160 90 155 Z',OUTLINE)
    a.path('M 111 46 L 205 46 L 220 145 L 100 145 Z',SOFT)
    for x in [125,160,195]:
        a.circle(x,68,9,FAINT);a.line(x,68,x+3,61,BRIGHT)
    a.rect(125,90,70,31,4,FAINT)
    if mode=='drive':
        a.poly([(134,109),(142,104),(149,110),(156,97),(164,110),(173,100),(187,103)],BRIGHT)
    elif mode=='delay':
        for x in [135,151,167,183]:a.line(x,99,x,112,SOFT)
        a.path('M 131 109 C 144 83 145 121 156 107 S 173 101 189 110',OUTLINE)
    elif mode=='mod':
        a.path('M 132 107 Q 143 87 154 107 T 177 107 T 190 107',OUTLINE)
    a.text(160,136,label,9,C['bright'],spacing=1.2,weight='600')
    a.circle(160,153,5,OUTLINE);a.dot(116,128,2.5,C['red'])
    for x in [98,223]: a.circle(x,106,3,SOFT)

# SHARED CONTROLS, each transparent and self-contained
controls={}
a=Art(160,140);unit_knob(a,80,64,36,42);a.text(80,121,'PARAMETER',10,spacing=1.2);save('controls','knob_arc',a)
a=Art(160,140);a.circle(80,63,36,OUTLINE);a.circle(80,63,28,SOFT);a.line(80,63,80,32,BRIGHT);a.text(80,120,'LEVEL',10,spacing=1.2);save('controls','knob_minimal',a)
a=Art(160,140);a.circle(80,65,36,FAINT);a.circle(80,65,28,FAINT);a.line(80,65,101,44,BRIGHT);a.path('M55 90 Q80 116 105 90',f'fill="none" stroke="{C["accent"]}" stroke-width="2.8"');a.text(80,122,'DRIVE',10,spacing=1.2);save('controls','knob_soft_arc',a)
a=Art(160,140);a.circle(61,65,29,OUTLINE);a.circle(100,65,29,OUTLINE);a.line(61,65,61,40,BRIGHT);a.line(100,65,84,48,BRIGHT);a.text(80,122,'DUAL',10,spacing=1.2);save('controls','knob_dual',a)
a=Art(160,140);a.line(24,69,136,69,FAINT);a.line(24,69,99,69,f'fill="none" stroke="{C["accent"]}" stroke-width="3.2" stroke-linecap="round"');a.circle(100,69,11,f'fill="{C["surface"]}" stroke="{C["bright"]}" stroke-width="1.6"');a.text(80,121,'HORIZONTAL',10,spacing=.8);save('controls','slider_horizontal',a)
a=Art(160,140);a.line(80,18,80,111,FAINT);a.line(80,70,80,111,f'fill="none" stroke="{C["accent"]}" stroke-width="2.7"');a.rect(68,60,24,17,5,f'fill="{C["surface"]}" stroke="{C["bright"]}" stroke-width="1.4"');a.text(115,73,'-6',11,C['bright'],anchor='start');save('controls','slider_vertical',a)
a=Art(160,140);a.rect(46,49,68,37,19,f'fill="{C["subtle"]}" stroke="{C["muted"]}" stroke-width="1.4"');a.circle(94,67,14,f'fill="{C["accent"]}" stroke="none"');a.text(80,118,'ON',10,C['muted']);save('controls','toggle_on',a)
a=Art(160,140);a.rect(46,49,68,37,19,FAINT);a.circle(66,67,14,f'fill="{C["surface"]}" stroke="{C["muted"]}" stroke-width="1.2"');a.text(80,118,'OFF',10,C['muted']);save('controls','toggle_off',a)
a=Art(160,140);a.circle(80,67,32,FAINT);a.path('M 80 42 L 80 64 M 64 52 A 23 23 0 1 0 96 52',BRIGHT);a.dot(103,43,3,C['green']);a.text(80,122,'BYPASS',10,spacing=1.2);save('controls','power_button',a)
a=Art(160,140);a.rect(16,44,128,46,10,FAINT);a.rect(19,47,59,40,8,f'fill="{C["raised"]}" stroke="none"');a.text(48,72,'A',12,C['bright']);a.text(111,72,'B',12,C['muted']);a.text(80,118,'A / B',10);save('controls','segmented_ab',a)
a=Art(160,140);a.rect(8,42,144,52,14,FAINT);a.text(23,73,'MR Clean Air',11,C['bright'],anchor='start');a.path('M 137 62 L 143 68 L 137 74',OUTLINE);a.text(80,120,'PRESETS',10);save('controls','preset_selector',a)
a=Art(160,140);colors=[C['red'],C['violet'],C['green'],C['yellow'],C['accent'],C['red']];
for i,c in enumerate(colors):a.dot(29+21*i,69,5,c)
a.text(80,121,'STATUS LEDS',10);save('controls','status_leds',a)
a=Art(160,140)
for j,x in enumerate([69,91]):
 for i in range(11):
  h=7+7*i;y=111-h
  sty=f'fill="{C["green"] if i<6+j else C["subtle"]}" stroke="none"'
  a.rect(x,y,10,5,1,sty)
a.text(80,124,'STEREO METER',10);save('controls','meter_stereo',a)
a=Art(160,140);a.rect(16,32,128,79,10,SOFT);a.path('M 25 77 C 43 77 47 58 59 58 S 74 95 85 72 S 111 47 135 57',f'fill="none" stroke="{C["accent"]}" stroke-width="2"');a.line(24,87,136,87,SOFT);a.line(24,47,136,47,SOFT);a.text(80,122,'EQ CURVE',10);save('controls','eq_curve',a)
a=Art(160,140);a.rect(14,30,132,82,10,SOFT);a.poly([(24,104),(48,47),(67,60),(91,86),(129,86),(138,104)],BRIGHT);
for x,y in [(48,47),(67,60),(91,86),(129,86)]:a.circle(x,y,3,FAINT)
a.text(80,123,'ENVELOPE',10);save('controls','envelope',a)
a=Art(160,140);a.rect(15,35,130,70,11,SOFT);pts=[]
for i in range(85):
 x=24+i*1.34;y=70+int(16*math.sin(i*.23)*math.sin(i*.91))
 pts.append((round(x,2),y))
a.poly(pts,BRIGHT);a.text(80,122,'WAVEFORM',10);save('controls','waveform',a)
a=Art(160,140);a.rect(30,20,100,100,9,FAINT)
for i in [55,80,105]:a.line(30,i,130,i,SOFT);a.line(i,20,i,120,SOFT)
a.circle(102,51,7,f'fill="{C["raised"]}" stroke="{C["bright"]}" stroke-width="1.6"');a.line(102,51,102,20,SOFT);a.line(102,51,130,51,SOFT);save('controls','xy_pad',a)
a=Art(160,140);a.rect(28,20,104,101,13,FAINT);a.text(80,40,'MODULE',10,C['bright'],spacing=1)
a.circle(55,83,16,OUTLINE);a.line(55,83,55,69,BRIGHT);a.circle(105,83,16,OUTLINE);a.line(105,83,113,74,BRIGHT);a.dot(119,40,2.8,C['green']);save('controls','module_tile',a)
a=Art(160,140)
for i,c in enumerate([C['red'],C['yellow'],C['green'],C['accent']]):
 x=35+i*30;a.circle(x,59,10,FAINT);a.circle(x,59,5,f'fill="{c}" stroke="none"')
a.path('M 35 84 L 125 84',SOFT);a.text(80,117,'PATCH PORTS',10);save('controls','patch_ports',a)
a=Art(160,140);a.rect(20,46,120,48,10,FAINT)
for i,t in enumerate(['MAIN','A','B']):
 x=30+i*38;a.text(x+15,75,t,10,C['bright'] if i==0 else C['muted']);
a.line(33,86,56,86,f'fill="none" stroke="{C["accent"]}" stroke-width="2"')
a.text(80,119,'TABS',10);save('controls','tabs',a)
a=Art(160,140);a.rect(20,30,120,82,12,FAINT);a.path('M37 75 L 62 75 L 76 56 L 93 92 L 107 75 L 127 75',BRIGHT);a.dot(76,56,3,C['accent']);a.text(80,126,'SIGNAL ROUTE',10);save('controls','signal_route',a)
a=Art(160,140);a.rect(28,25,104,92,12,FAINT);a.rect(42,40,76,18,4,SOFT);a.text(80,53,'-12.6 dB',12,C['bright']);a.line(42,73,118,73,FAINT);a.rect(42,71,43,4,2,f'fill="{C["accent"]}" stroke="none"');a.text(80,125,'READOUT',10);save('controls','parameter_readout',a)
a=Art(160,140);a.circle(52,70,23,FAINT);a.circle(108,70,23,FAINT);a.line(52,70,38,56,BRIGHT);a.line(108,70,121,57,BRIGHT);a.path('M75 70 L85 70 M80 65 L80 75',SOFT);a.text(80,123,'MID / SIDE',10);save('controls','dual_channel',a)

# MR RIG: amplifier, cabinet, stompboxes and microphones
# All illustrations remain transparent, 320 x 200 artboard.
a=Art();a.path('M34 58 Q 35 47 45 47 L 275 47 Q 286 47 286 58 L 286 152 Q 286 161 277 161 L 43 161 Q 34 161 34 152 Z');a.rect(44,57,232,94,8,FAINT)
a.path('M48 90 L272 90',FAINT);a.path('M48 119 L272 119',SOFT)
for x in [61,88,115,142,169,196,223]:a.circle(x,105,6,OUTLINE);a.line(x,105,x+2,101,BRIGHT)
a.rect(57,66,103,17,3,FAINT);a.text(108,79,'MR   /   RIG',11,C['bright'],spacing=2)
for x in range(177,262,8):a.line(x,65,x,82,SOFT)
for x in range(51,274,4):a.line(x,127,x,143,SOFT)
a.rect(47,151,32,8,1,FAINT);a.rect(245,151,29,8,1,FAINT);a.dot(258,105,3,C['red']);save('rig','amp_head',a)
a=Art();a.path('M76 32 L244 32 Q255 32 256 42 L265 172 L55 172 L64 43 Q 64 32 76 32 Z');a.rect(72,44,176,36,5,FAINT);a.text(160,58,'MR',10,C['bright'],spacing=2)
for x in [90,111,132,153,174,195,216]:a.circle(x,68,4,OUTLINE);a.line(x,68,x,64,BRIGHT)
a.rect(73,88,174,67,6,FAINT);a.circle(160,122,29,OUTLINE);a.circle(160,122,20,SOFT);a.dot(160,122,4,C['subtle'])
for i in range(76,247,5):a.line(i,90,i,154,SOFT)
a.line(69,160,250,160,FAINT);save('rig','combo_amp',a)
a=Art();a.path('M65 32 L255 32 Q263 32 265 42 L265 176 L55 176 L55 42 Q57 32 65 32 Z');a.rect(67,44,186,119,8,FAINT)
for x,y in [(112,81),(208,81),(112,132),(208,132)]:
 a.circle(x,y,33,OUTLINE);a.circle(x,y,23,SOFT);a.circle(x,y,5,SOFT)
for x in range(72,253,7): a.line(x,47,x,160,SOFT)
a.rect(79,167,20,8,2,FAINT);a.rect(221,167,20,8,2,FAINT);save('rig','cabinet_4x12',a)
a=Art();a.path('M51 46 Q51 34 63 34 L257 34 Q269 34 269 46 L269 170 L51 170 Z');a.rect(63,46,194,113,5,FAINT)
for x in [111,209]:a.circle(x,103,40,OUTLINE);a.circle(x,103,31,SOFT);a.circle(x,103,7,SOFT)
for x in range(69,256,8):a.line(x,49,x,158,SOFT)
a.text(160,173,'MR',9,C['bright'],spacing=1);save('rig','cabinet_2x12',a)
a=Art();pedal_body(a,'DRIVE','drive');save('rig','pedal_drive',a)
a=Art();pedal_body(a,'ECHO','delay');save('rig','pedal_delay',a)
a=Art();pedal_body(a,'MODULATION','mod');save('rig','pedal_modulation',a)
a=Art();a.path('M103 49 Q103 31 122 26 L143 23 Q160 22 161 40 L161 101 Q161 113 147 121 L117 121 Q103 118 103 103 Z');a.rect(110,48,43,56,12,FAINT)
for y in range(55,99,7):a.line(112,y,153,y,SOFT)
a.line(131,121,131,167,OUTLINE);a.path('M 130 166 L 207 178',OUTLINE);a.circle(207,178,4,FAINT);a.rect(124,118,14,9,2,FAINT);save('rig','mic_dynamic',a)
a=Art();a.rect(133,21,55,115,23,OUTLINE);a.rect(142,30,37,67,17,FAINT)
for x in range(146,178,6):a.line(x,35,x,91,SOFT)
a.path('M149 98 L149 117 L172 117 L172 98',FAINT);a.line(160,136,160,172,OUTLINE);a.path('M160 165 L205 178',OUTLINE);a.circle(205,178,4,FAINT);a.rect(151,117,18,17,3,FAINT);save('rig','mic_ribbon',a)
a=Art();a.path('M119 34 Q119 23 135 23 L185 23 Q201 23 201 34 L201 151 Q201 165 185 165 L135 165 Q119 165 119 151 Z');a.path('M134 40 L134 153 M186 40 L186 153',FAINT)
for x in [150,160,170]:a.path(f'M{x} 64 C {x+9} 79 {x-10} 97 {x} 111 S{x+7} 135 {x} 144',OUTLINE)
a.rect(126,32,68,7,2,SOFT);a.rect(126,152,68,7,2,SOFT);save('rig','valve_tube',a)
a=Art();a.rect(37,60,246,89,11,OUTLINE);a.rect(47,69,226,70,7,FAINT)
for x in [66,112,158,204,250]:a.circle(x,106,13,OUTLINE);a.line(x,106,x+4,94,BRIGHT)
a.rect(53,77,80,9,3,SOFT);a.text(93,85,'MR GATE',9,C['bright'])
for i in range(19):
 x=55+i*11
 a.line(x,131,x,133+4*math.sin(i),SOFT)
save('rig','rack_processor',a)
a=Art();a.rect(44,82,232,58,12,OUTLINE);a.rect(49,87,222,48,9,FAINT)
for x in [70,109,148,187,226]:a.circle(x,106,8,OUTLINE);a.line(x,106,x+3,102,BRIGHT)
a.path('M48 63 L 270 63 L250 81 L66 81 Z',FAINT)
for x in range(60,264,10):a.line(x,88,x,132,SOFT)
save('rig','pedalboard',a)

# MR REVERB: original pictograms of reverberation hardware and acoustic spaces
a=Art();a.rect(44,37,232,126,8,OUTLINE);a.rect(57,49,206,102,3,FAINT)
for x,y in [(65,57),(255,57),(65,143),(255,143)]:a.circle(x,y,5,SOFT)
a.line(77,100,244,100,FAINT)
for k in range(6):
 y=66+k*14;a.path(f'M82 {y} Q155 {y-16} 237 {y}',SOFT)
a.text(160,175,'PLATE',9,C['bright'],spacing=2);save('reverb','plate_unit',a)
a=Art();a.rect(43,70,234,64,9,OUTLINE);a.rect(53,80,214,44,5,FAINT)
for x in range(66,259,11):a.path(f'M{x} 98 q5 -15 10 0 t10 0',FAINT)
for x in [54,266]:a.circle(x,102,8,OUTLINE)
a.line(59,134,59,146,FAINT);a.line(263,134,263,146,FAINT);a.text(160,167,'SPRING TANK',9,C['bright'],spacing=1.5);save('reverb','spring_tank',a)
def space_art(kind):
 a=Art();a.path('M29 163 L291 163 L291 41 L29 41 Z',FAINT)
 if kind in ['hall','chamber','room','cathedral']:
  a.path('M29 163 L106 112 L214 112 L291 163 M29 41 L106 69 L214 69 L291 41',OUTLINE)
  a.line(106,69,106,112,FAINT);a.line(214,69,214,112,FAINT)
  a.line(160,69,160,112,SOFT)
  for t in [0.23,0.49,0.75]:
   y=163-51*t;a.line(29+77*t,y,291-77*t,y,SOFT)
 if kind=='hall':
  for x in [65,257]:a.path(f'M{x-12} 160 L{x-12} 70 Q{x} 47 {x+12} 70 L{x+12} 160',OUTLINE)
  a.path('M142 111 L142 87 Q160 64 178 87 L178 111',BRIGHT)
 if kind=='chamber':
  a.path('M113 111 L113 89 L206 89 L206 111',BRIGHT)
  for x in [80,240]: a.line(x,63,x,134,FAINT)
 if kind=='room':
  a.rect(120,84,80,27,2,FAINT);a.path('M137 84 L137 100 L184 100 L184 84',SOFT)
  for x in [71,250]:a.path(f'M{x} 156 L{x} 93',FAINT)
 if kind=='cathedral':
  for x in [62,258]:
   a.path(f'M{x-19} 162 L{x-19} 79 Q{x-19} 59 {x} 50 Q{x+19} 59 {x+19} 79 L{x+19} 162',OUTLINE)
  a.path('M123 110 L123 91 Q160 40 197 91 L197 110',BRIGHT)
 return a
for typ in ['hall','chamber','room','cathedral']:save('reverb',typ+'_space',space_art(typ))
a=Art();a.rect(42,37,236,128,9,FAINT)
for j in range(8):
 x=59+j*29
 a.path(f'M{x} 131 C{x+8} 101 {x-4} 77 {x+9} 67',FAINT if j%2 else OUTLINE)
for x,y in [(71,70),(134,107),(196,77),(252,112)]:a.circle(x,y,4,BRIGHT)
a.path('M66 147 Q160 126 254 147',SOFT);save('reverb','diffusion_field',a)
a=Art();a.line(37,106,281,106,SOFT)
for i in range(100):
 x=40+2.4*i
 amp=35*math.exp(-i/30)*math.sin(i*2.63)
 a.line(round(x,2),106-amp,round(x,2),106+amp,OUTLINE if i<18 else FAINT)
a.text(160,170,'IMPULSE RESPONSE',9,C['bright'],spacing=1.6);save('reverb','impulse_response',a)
a=Art();a.rect(32,29,256,139,8,FAINT)
a.dot(67,99,7,C['bright']);a.dot(252,99,7,C['accent']);a.line(75,99,244,99,SOFT)
for i in range(7):
 x=91+i*23;a.line(x,64,x,135,FAINT);a.circle(x,99,5 if i<4 else 3,BRIGHT if i<4 else FAINT)
a.path('M67 80 Q160 35 252 80',SOFT);a.path('M67 121 Q160 167 252 121',SOFT);save('reverb','early_reflections',a)
a=Art();a.rect(38,31,244,134,10,FAINT)
for i in range(6):
 s=1-i*.12;x=70+i*25;radius=26*s
 a.path(f'M{x} 121 a{radius} {radius} 0 0 1 {radius*2} 0',OUTLINE if i<3 else FAINT)
a.line(57,127,266,127,SOFT);a.text(160,178,'ECHO',9,C['bright'],spacing=1.5);save('reverb','echo_taps',a)
a=Art();a.rect(32,40,256,112,10,OUTLINE);a.rect(46,52,106,27,4,FAINT);a.text(99,70,'MR   M7',12,C['bright'],spacing=2)
for x in [185,219,254]:a.circle(x,69,10,OUTLINE);a.line(x,69,x+3,62,BRIGHT)
a.rect(47,95,228,45,4,FAINT)
for x in range(59,267,13):a.line(x,105,x,131,SOFT)
a.dot(273,48,3,C['red']);save('reverb','digital_reverb',a)

# Icons (24x24; same palette, crisp linework)
def icon(name,fn):
 a=Art(24,24);fn(a);save('icons',name,a)
icon('chevron_left',lambda a:a.path('M15 5 L8 12 L15 19',OUTLINE))
icon('chevron_right',lambda a:a.path('M9 5 L16 12 L9 19',OUTLINE))
icon('close',lambda a:a.path('M6 6 L18 18 M18 6 L6 18',OUTLINE))
icon('menu',lambda a:a.path('M5 7 L19 7 M5 12 L19 12 M5 17 L19 17',OUTLINE))
icon('plus',lambda a:a.path('M12 5 L12 19 M5 12 L19 12',OUTLINE))
icon('settings',lambda a:a.add(f'<circle cx="12" cy="12" r="6.5" {OUTLINE}/><circle cx="12" cy="12" r="2" {FAINT}/><path d="M12 2 V5 M12 19 V22 M2 12 H5 M19 12 H22 M5 5 L7 7 M17 17 L19 19 M19 5 L17 7 M7 17 L5 19" {FAINT}/>'))
icon('power',lambda a:a.path('M12 3 L12 12 M6.5 6.5 A8 8 0 1 0 17.5 6.5',BRIGHT))
icon('routing',lambda a:a.path('M4 6 H11 L15 10 H20 M16 7 L20 10 L16 13 M4 18 H10 L14 14 H20',OUTLINE))
icon('cabinet',lambda a:a.add(f'<rect x="4" y="3" width="16" height="18" rx="2" {OUTLINE}/><circle cx="9" cy="9" r="3" {FAINT}/><circle cx="15" cy="9" r="3" {FAINT}/><circle cx="9" cy="16" r="3" {FAINT}/><circle cx="15" cy="16" r="3" {FAINT}/>'))
icon('mic',lambda a:a.path('M9 3 Q9 1 12 1 Q15 1 15 4 V12 Q15 16 12 16 Q9 16 9 12 Z M6 11 Q6 20 12 20 Q18 20 18 11 M12 20 V23',OUTLINE))
icon('reverb',lambda a:a.path('M3 16 Q7 3 12 16 T21 16 M5 21 Q12 11 19 21',OUTLINE))
icon('pedal',lambda a:a.add(f'<rect x="5" y="2" width="14" height="20" rx="3" {OUTLINE}/><circle cx="9" cy="7" r="2" {FAINT}/><circle cx="15" cy="7" r="2" {FAINT}/><circle cx="12" cy="16" r="3" {BRIGHT}/>'))
icon('sine',lambda a:a.path('M2 13 Q7 2 12 13 T22 13',BRIGHT))
icon('wave',lambda a:a.path('M2 12 L5 12 L7 4 L11 20 L14 8 L17 14 L22 14',BRIGHT))
icon('swap',lambda a:a.path('M3 7 H21 L17 3 M21 7 L17 11 M21 17 H3 L7 13 M3 17 L7 21',OUTLINE))
icon('link',lambda a:a.path('M9 8 L15 16 M8 15 L5 18 Q3 20 5 22 M16 9 L19 6 Q21 4 19 2 M6 12 Q2 8 7 4 Q11 1 14 4 M18 12 Q22 16 17 20 Q13 23 10 20',OUTLINE))
icon('headphones',lambda a:a.path('M4 13 V11 A8 8 0 0 1 20 11 V13 M4 13 H8 V20 H4 Z M16 13 H20 V20 H16 Z',OUTLINE))
icon('save',lambda a:a.path('M4 3 H17 L21 7 V21 H3 V3 Z M7 3 V10 H17 V3 M7 21 V14 H17 V21',OUTLINE))
icon('random',lambda a:a.path('M2 7 H5 Q10 7 14 15 Q17 20 22 17 M18 13 L22 17 L18 21 M2 17 H5 Q10 17 14 9 Q17 4 22 7 M18 3 L22 7 L18 11',OUTLINE))

# Theme token files
(ROOT/'tokens').mkdir(exist_ok=True)
(ROOT/'tokens'/'mr-plugin-colors.json').write_text(json.dumps({"name":"MR Studio / Native Plugin Dark Outline","colors":C,"density":{"baseUnit":4,"headerHeight":48,"panelRadius":15,"cardRadius":12,"gap":12,"hairline":1,"controlTouchTarget":32},"states":{"disabledOpacity":0.34,"hoverOutline":"#DAE5F4","focus":"#91BBEE","activeIndicator":"#91BBEE","warningIndicator":"#E2C070"}},ensure_ascii=False,indent=2),encoding='utf8')
(ROOT/'tokens'/'mr-plugin-tokens.css').write_text(':root {\n'+''.join(f'  --mr-{k}: {v};\n' for k,v in C.items())+'''  --mr-radius-panel: 15px;
  --mr-radius-card: 12px;
  --mr-header-height: 48px;
  --mr-line: 1px;
  --mr-space: 4px;
}
.mr-native-panel {background: var(--mr-window);color: var(--mr-bright);font-family: Inter, Segoe UI, sans-serif;}
.mr-native-card {background: var(--mr-surface);border-radius: var(--mr-radius-card);}
.mr-native-text-muted {color: var(--mr-muted);}
.mr-native-control[aria-disabled='true'] {opacity: .34;}
.mr-native-control:focus-visible {outline: 2px solid var(--mr-accent);outline-offset: 2px;}
''',encoding='utf8')

# embed transparently authored components in large prototype SVGs

def embed(id,x,y,w,h):
 d=ASSETS[id]
 return f'<svg x="{x}" y="{y}" width="{w}" height="{h}" viewBox="0 0 {d["width"]} {d["height"]}" overflow="visible">{d["fragment"]}</svg>'

def common_frame(a,product,sub,preset):
 a.rect(0,0,a.w,a.h,0,f'fill="{C["canvas"]}" stroke="none"')
 a.rect(20,20,a.w-40,a.h-40,18,f'fill="{C["window"]}" stroke="{C["subtle"]}" stroke-width=".6"')
 a.rect(20,20,a.w-40,61,12,f'fill="{C["raised"]}" stroke="none"');a.rect(20,65,a.w-40,16,0,f'fill="{C["raised"]}" stroke="none"')
 a.circle(64,50,14,OUTLINE);a.path('M55 46 L64 57 L73 46 M55 46 L73 46 M64 57 L64 61',BRIGHT);a.text(85,46,product,20,C['bright'],anchor='start',spacing=1.1,weight='600');a.text(85,64,sub,9,C['muted'],anchor='start',spacing=1.6)
 a.rect(362,36,260,31,15,FAINT);a.text(382,57,preset,12,C['bright'],anchor='start');a.path('M586 48 L591 53 L586 58',FAINT)
 for i,label in enumerate(['MAIN','A','B','FX']):
  x=840+i*72;a.text(x,55,label,12,C['bright'] if i==0 else C['muted'],anchor='start')
 a.line(840,66,876,66,f'fill="none" stroke="{C["bright"]}" stroke-width="1.6"')
 a.text(1175,55,'●',15,C['green']);a.text(1202,55,'?',16,C['muted'])

def card(a,x,y,w,h,title,tag=None):
 a.rect(x,y,w,h,17,f'fill="{C["surface"]}" stroke="none"')
 a.text(x+23,y+31,title,12,C['bright'],anchor='start',spacing=.6,weight='600')
 if tag:a.text(x+w-21,y+31,tag,10,C['muted'],anchor='end',spacing=.8)

# Rig compact panel
p=Art(1280,720);common_frame(p,'MR RIG','AMP · CAB · FX','Clean Edge / No. 04')
card(p,40,100,268,455,'SIGNAL CHAIN','04 ACTIVE')
for i,(label,sub,typ) in enumerate([('INPUT','GATE','rack_processor'),('DRIVE','Soft clipping','pedal_drive'),('AMP','Clean edge','amp_head'),('CAB','4 × 12 Modern','cabinet_4x12')]):
 y=143+98*i;p.rect(56,y,238,83,12,f'fill="{C["raised"] if i==2 else C["deep"]}" stroke="{C["accent"] if i==2 else C["subtle"]}" stroke-width="{1 if i==2 else .6}"');p.text(72,y+28,label,12,C['bright'],anchor='start',weight='600');p.text(72,y+48,sub,10,C['muted'],anchor='start');p.add(embed('rig/'+typ,196,y+1,91,71));p.dot(275,y+20,3,C['green'] if i<4 else C['muted']);
 if i<3:p.line(175,y+83,175,y+98,FAINT)
card(p,321,100,572,455,'AMPLIFIER','HEAD / CLEAN 01')
p.add(embed('rig/amp_head',352,151,505,268))
p.line(360,422,852,422,SOFT)
for i,(lab,val,ang) in enumerate([('GAIN','3.4',18),('BASS','5.2',-14),('MID','4.5',24),('TREBLE','6.8',54),('MASTER','-6.0',5)]):
 x=394+100*i;unit_knob(p,x,467,24,ang);p.text(x,513,lab,10,C['muted'],spacing=.8);p.text(x,535,val,12,C['bright'])
card(p,907,100,331,455,'CABINET / MICROPHONE','IR 002')
p.add(embed('rig/cabinet_4x12',929,155,287,188));p.add(embed('rig/mic_dynamic',1121,229,90,73))
p.line(928,351,1216,351,SOFT)
for i,(lab,val) in enumerate([('MIC','Dynamic 57'),('POSITION','32% off-axis'),('DISTANCE','8 cm')]):
 y=378+i*49;p.text(934,y,lab,10,C['muted'],anchor='start',spacing=.8);p.text(1208,y,val,12,C['bright'],anchor='end');p.line(934,y+15,1210,y+15,SOFT)
p.rect(40,569,1198,130,15,f'fill="{C["deep"]}" stroke="none"')
p.text(62,596,'POST FX',12,C['bright'],anchor='start',spacing=.8,weight='600')
for i,typ in enumerate(['pedal_drive','pedal_delay','pedal_modulation']):
 x=186+i*172;p.add(embed('rig/'+typ,x,588,76,95));p.text(x+92,641,['DRIVE','ECHO','MOD'][i],11,C['bright'],anchor='start');p.dot(x+93,657,3,C['green'] if i!=2 else C['muted'])
p.line(792,590,792,680,SOFT);p.text(819,608,'OUTPUT',10,C['muted'],anchor='start');p.text(819,643,'-6.2 dB',20,C['bright'],anchor='start');p.text(1080,608,'OVERSAMPLING',10,C['muted'],anchor='start');p.text(1080,643,'4×',20,C['bright'],anchor='start')
(ROOT/'previews').mkdir(exist_ok=True)
(ROOT/'previews'/'MR_Rig_GUI_concept.svg').write_text(p.svg(),encoding='utf8')

# Reverb compact panel
p=Art(1280,720);common_frame(p,'MR REVERB','SPACE · IMPULSE · TAIL','Large Hall / Warm')
card(p,40,100,408,426,'SPACE ENGINE','TYPE / HALL')
p.add(embed('reverb/hall_space',69,164,349,237))
p.rect(70,419,348,77,9,f'fill="{C["deep"]}" stroke="none"');p.text(91,447,'LARGE HALL',14,C['bright'],anchor='start',weight='600',spacing=1);p.text(91,473,'Warm / Diffuse / Concert',11,C['muted'],anchor='start')
card(p,463,100,489,426,'EARLY REFLECTIONS','DIFFUSION / IR')
p.add(embed('reverb/impulse_response',488,139,440,204));p.line(490,350,919,350,SOFT)
for i,(lab,degree) in enumerate([('PRE-DELAY',15),('DIFFUSION',46),('DAMPING',-19)]):
 x=546+i*148;unit_knob(p,x,401,29,degree);p.text(x,461,lab,10,C['muted'],spacing=.8);p.text(x,488,['28 ms','72%','5.6 kHz'][i],13,C['bright'])
card(p,968,100,270,426,'TAIL / OUTPUT','STEREO')
for i,(lab,val,angle) in enumerate([('DECAY','4.8 s',17),('SIZE','87%',43),('MIX','24%',-25)]):
 x=1031+(i%2)*135;y=205+(i//2)*142;unit_knob(p,x,y,35,angle);p.text(x,y+57,lab,10,C['muted'],spacing=1);p.text(x,y+77,val,13,C['bright'])
p.line(993,440,1213,440,SOFT);p.text(992,472,'WIDTH',10,C['muted'],anchor='start');p.text(1213,472,'100%',13,C['bright'],anchor='end');p.line(991,497,1212,497,FAINT);p.line(991,497,1190,497,f'fill="none" stroke="{C["accent"]}" stroke-width="3"');p.circle(1190,497,7,f'fill="{C["deep"]}" stroke="{C["bright"]}" stroke-width="1.4"')
p.rect(40,540,1198,159,14,f'fill="{C["deep"]}" stroke="none"');p.text(64,570,'REVERB CHARACTER',12,C['bright'],anchor='start',weight='600');
for i,(name,sel) in enumerate([('ROOM',False),('CHAMBER',False),('HALL',True),('PLATE',False),('SPRING',False)]):
 x=75+172*i;p.rect(x,585,151,89,9,f'fill="{C["raised"] if sel else C["surface"]}" stroke="{C["accent"] if sel else C["subtle"]}" stroke-width="{1.5 if sel else .7}"');p.text(x+75,625,name,11,C['bright'] if sel else C['muted'],spacing=.8);p.dot(x+75,649,3.2,C['accent'] if sel else C['subtle'])
p.line(973,575,973,681,SOFT);p.text(1000,603,'OUTPUT',10,C['muted'],anchor='start');p.text(1000,638,'-2.6 dB',21,C['bright'],anchor='start')
(ROOT/'previews'/'MR_Reverb_GUI_concept.svg').write_text(p.svg(),encoding='utf8')

# Render examples
for name in ['MR_Rig_GUI_concept','MR_Reverb_GUI_concept']:
 svg=ROOT/'previews'/f'{name}.svg'
 cairosvg.svg2png(url=str(svg),write_to=str(ROOT/'previews'/f'{name}.png'),output_width=1600,output_height=900)

# Contact sheet with custom labels and thumbnails. Captures all assets and showcases categories.
fontreg='/usr/share/fonts/opentype/inter/Inter-Regular.otf';fontmed='/usr/share/fonts/opentype/inter/Inter-SemiBold.otf'
if not Path(fontreg).exists():fontreg='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
F=lambda s,bold=False:ImageFont.truetype(fontmed if bold else fontreg,s)
cols=5;cardw=274;cardh=216;gap=16;left=42;right=42
categories=[('controls','01  /  SHARED CONTROLS'),('rig','02  /  MR RIG HARDWARE'),('reverb','03  /  MR REVERB SPACES'),('icons','04  /  ICONS 24 PX')]
rows_bycat={cat:[(key,v) for key,v in ASSETS.items() if key.startswith(cat+'/')] for cat,_ in categories}
H=148+sum(74+math.ceil(len(rows_bycat[cat])/cols)*(cardh+gap)+18 for cat,_ in categories)
W=left+cols*cardw+(cols-1)*gap+right
im=Image.new('RGB',(W,H),C['canvas']);dr=ImageDraw.Draw(im)
dr.text((45,43),'MR STUDIO',font=F(39,True),fill=C['bright']);dr.text((46,95),'NATIVE PLUGIN SVG KIT  /  DESIGN SYSTEM 01',font=F(15),fill=C['muted'])
y=151
for category,label in categories:
 dr.text((left,y),label,font=F(17,True),fill=C['bright']);y+=48
 for index,(key,meta) in enumerate(rows_bycat[category]):
  col=index%cols;row=index//cols;x=left+col*(cardw+gap);cy=y+row*(cardh+gap)
  dr.rounded_rectangle((x,cy,x+cardw,cy+cardh),radius=17,fill=C['surface'])
  sv=(ROOT/meta['path']).read_bytes()
  dims=(238,155) if category!='icons' else (92,92)
  png=cairosvg.svg2png(bytestring=sv,output_width=dims[0],output_height=dims[1]);render=Image.open(io.BytesIO(png)).convert('RGBA')
  if category=='icons':px=x+(cardw-dims[0])//2;py=cy+28
  else:px=x+(cardw-dims[0])//2;py=cy+8
  im.paste(render,(px,py),render)
  title=key.split('/')[1].replace('_',' ').upper()
  dr.text((x+16,cy+cardh-29),title,font=F(12,True),fill=C['bright'])
 y+=math.ceil(len(rows_bycat[category])/cols)*(cardh+gap)+18
im.save(ROOT/'previews'/'SVG_Components_Overview.png',optimize=True)

# Browser preview of the entire kit, no external dependencies, all files remain separately addressable.
items='\n'.join(f'''<figure><div class="art"><img src="../{meta['path']}" alt="{key}" loading="lazy"></div><figcaption>{key.split('/')[1].replace('_',' ')}</figcaption></figure>''' for key,meta in ASSETS.items())
htmlpage='''<!doctype html><html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>MR Studio — SVG GUI Kit</title><style>
:root{--bg:#111720;--panel:#222C3C;--txt:#DAE5F4;--muted:#7188AB;--accent:#91BBEE}*{box-sizing:border-box}body{background:var(--bg);color:var(--txt);font-family:Inter,Segoe UI,sans-serif;margin:0;padding:35px;max-width:1600px;margin-inline:auto}h1{font-weight:550;font-size:30px;letter-spacing:.03em;margin:0}p{color:var(--muted);font-size:14px}nav{display:flex;gap:14px;flex-wrap:wrap;padding:20px 0 25px}nav a{color:var(--accent);font-size:13px}h2{font-size:16px;letter-spacing:.12em;margin:42px 0 16px}section{display:grid;grid-template-columns:repeat(auto-fill,minmax(210px,1fr));gap:12px}figure{margin:0;background:var(--panel);border-radius:12px;padding:12px;text-align:center}figure .art{height:142px;display:flex;align-items:center;justify-content:center}figure img{width:100%;height:100%;object-fit:contain}figure img[src*="/icons/"]{width:65px;height:65px}figcaption{font-size:12px;color:var(--txt);padding:10px 0 2px;text-transform:uppercase;letter-spacing:.06em}.concept{width:100%;border-radius:14px;margin-bottom:20px;border:1px solid #344259}
</style></head><body><h1>MR Studio / Native Plugin SVG Kit</h1><p>Векторная библиотека для нативных эффектов и инструментов. Все объекты представлены самостоятельными SVG.</p><nav><a href="#controls">Controls</a><a href="#rig">MR Rig</a><a href="#reverb">MR Reverb</a><a href="#icons">Icons</a><a href="#concepts">GUI concepts</a></nav>'''
for category,label in categories:
 htmlpage+=f'<h2 id="{category}">{label}</h2><section>'
 htmlpage+='\n'.join(f'<figure><div class="art"><img src="../{meta["path"]}" alt="{key}" loading="lazy"></div><figcaption>{key.split("/")[1].replace("_"," ")}</figcaption></figure>' for key,meta in rows_bycat[category])+'</section>'
htmlpage+='<h2 id="concepts">05 / EXAMPLE PLUGIN PANELS</h2><img class="concept" src="MR_Rig_GUI_concept.svg" alt="MR Rig concept"><img class="concept" src="MR_Reverb_GUI_concept.svg" alt="MR Reverb concept"></body></html>'
(ROOT/'previews'/'index.html').write_text(htmlpage,encoding='utf8')

(ROOT/'README.md').write_text(f'''# MR Studio — Native Plugin SVG Kit 01

Единый визуальный язык для нативных плагинов **MR Studio**. Оригинальная система, вдохновлённая предоставленным скриншотом Synth Anthology 4: графитовые панели, холодные светло-голубые контуры, минимум деталей, линейные иллюстрации аппаратуры, небольшие активные индикаторы. **Не копия интерфейса или изображений исходного продукта.**

## Содержимое

- `svg/controls/` — {len(rows_bycat['controls'])} примера регуляторов, переключателей, индикации и графиков.
- `svg/rig/` — {len(rows_bycat['rig'])} линейных изображений гитарного оборудования и педалей для **MR Rig**.
- `svg/reverb/` — {len(rows_bycat['reverb'])} изображений аппаратов, IR и акустических пространств для **MR Reverb**.
- `svg/icons/` — {len(rows_bycat['icons'])} пиктограмм с viewBox 24×24.
- `tokens/` — палитра и базовые CSS/JSON design tokens.
- `previews/` — HTML-каталог, PNG-контактный лист и два полноразмерных концепта интерфейсов (SVG + PNG).

**Всего: {len(ASSETS)} отдельных SVG элементов + 2 концепта панелей.**

## Принципы интеграции

1. Иллюстрации оборудования и иконки — статическая векторная графика с прозрачным фоном. SVG не содержат растровых вложений, фильтров, внешних шрифтов и внешних зависимостей.
2. Интерактивные элементы (`knob`, `slider`, `meter`, `toggle`) — **визуальные образцы состояний**, не заменяют полноценные контролы и не реализуют ввод. Для реального GUI лучше рисовать контур программно, а SVG использовать как стиль и референс либо менять геометрию по параметрам.
3. ViewBox для controls = 160×140; rig/reverb = 320×200; icons = 24×24. Масштабирование сохраняет линии, но для миниатюр необходимы отдельные LOD-версии или предельная плотность деталей.
4. Не встраивать названия параметров из примера как статический текст в интерактивные контролы — реальные подписи должны приходить из UI и состояния плагина.
5. Цвета заданы прямо в SVG ради предсказуемого рендера (в том числе в Qt SVG). При адаптации темы можно заменить шестнадцатеричные значения на этапе загрузки или конвертировать SVG в пути и назначить цвета средствами хоста.
6. **UI/аудиопотоки разделять**: отображение SVG и управление состоянием работают на UI-потоке, передача параметров в DSP — штатным realtime-safe механизмом. Не вводить рендеринг SVG в audio callback.
7. Активные элементы: акцент `#91BBEE` для выбранного, `#7AB69E` для включено, `#E68F88` — сигнализирующий индикатор. Не превращать весь интерфейс в ярко-синее свечение.
8. Для компактности ориентироваться на шаг 4 px, шапку ~48 px, радиус больших карточек 12–15 px; основной контур 1–1.6 px, подписи 10–12 px при проектном масштабе.
9. Для MR Rig: усилитель и кабинет — крупная контурная схема (не фотореализм), рядом реальные параметры; цепочка эффектов — компактными карточками.
10. Для MR Reverb: визуализация комнаты или устройства / центральная форма IR / простое управление decay, predelay, diffusion, mix. Отдельные send-режимы должны подключаться к маршрутизации DAW, не кодироваться в графике.

## Палитра

| Token | Hex | Применение |
| --- | --- | --- |
| canvas | {C['canvas']} | основной фон |
| window | {C['window']} | окно плагина |
| surface | {C['surface']} | панель/карточка |
| raised | {C['raised']} | активная карточка/шапка |
| deep | {C['deep']} | вложенный модуль |
| bright | {C['bright']} | контуры/основной текст |
| stroke | {C['stroke']} | основная контурная иллюстрация |
| muted | {C['muted']} | второстепенные линии/подписи |
| subtle | {C['subtle']} | сетки, неактивные разделители |
| accent | {C['accent']} | активный параметр |
| red | {C['red']} | индикаторы/предупреждения |
| green | {C['green']} | статус On |

## Открыть

- `previews/SVG_Components_Overview.png` — единый каталог.
- `previews/index.html` — локальная интерактивно масштабируемая галерея без JS.
- `previews/MR_Rig_GUI_concept.svg` и `previews/MR_Reverb_GUI_concept.svg` — примеры возможных экранов, **не готовая архитектура UI**.

## Статус

Концепт библиотеки. Перед утверждением production-дизайна проверить в реальном масштабе экрана, доступность (контраст и размер hit areas), выровнять дизайн под фактическую структуру DSP и UX каждого плагина. Генератор `build_kit.py` позволяет пересобирать все элементы и изображения.
''',encoding='utf8')

manifest={"name":"MR Studio Native Plugin SVG Kit","version":"0.1","total_asset_svg":len(ASSETS),"assets":[{"id":k,"file":d['path'],"size":[d['width'],d['height']]} for k,d in ASSETS.items()]}
(ROOT/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')

zip_path=ROOT.parent/'MR_Plugin_GUI_SVG_Kit_01.zip'
with zipfile.ZipFile(zip_path,'w',zipfile.ZIP_DEFLATED,compresslevel=7) as z:
 for f in ROOT.rglob('*'):
  if f.is_file():z.write(f,f.relative_to(ROOT.parent))
print('Assets:',len(ASSETS),'ZIP:',zip_path,'size:',zip_path.stat().st_size,'Contact sheet:',im.size)

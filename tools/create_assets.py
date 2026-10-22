#!/usr/bin/env python3
from pathlib import Path
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[1]
def menu():
 im=Image.new('RGBA',(25,25),(0,0,0,0));d=ImageDraw.Draw(im)
 d.rectangle((1,1,23,23),fill='#000055',outline='white',width=2)
 d.line((3,3,21,3),fill='#AAAAAA');d.line((3,3,3,21),fill='#AAAAAA')
 for x,y in [(6,6),(7,6),(10,6),(11,6),(5,7),(6,7),(7,7),(8,7),(9,7),(10,7),(11,7),(12,7),(6,8),(7,8),(8,8),(9,8),(10,8),(11,8),(7,9),(8,9),(9,9),(10,9),(8,10),(9,10)]:d.point((x,y),fill='#FF0055')
 d.rectangle((5,14,19,16),fill='#000000');d.rectangle((5,14,16,15),fill='#55FF55')
 d.rectangle((5,19,19,21),fill='#000000');d.rectangle((5,19,13,20),fill='#55FFFF')
 return im
menu().save(ROOT/'resources/images/menu_icon.png')
if (ROOT/'shots/demo.png').exists():
 image=Image.open(ROOT/'shots/demo.png').convert('RGB')
 for size in (80,144):image.resize((size,size),Image.Resampling.LANCZOS).save(ROOT/f'icon_{size}x{size}.png')
print('Quest launcher and store icons generated.')

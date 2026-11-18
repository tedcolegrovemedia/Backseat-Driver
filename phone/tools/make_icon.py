import random, sys
from PIL import Image, ImageDraw, ImageFilter
N=1024; K=4; W=N*K
random.seed(7)
img=Image.new("RGB",(W,W),(22,22,22))
# asphalt grain, like the display's night texture
grain=Image.effect_noise((N,N),18).resize((W,W),Image.NEAREST)
img=Image.blend(img,Image.merge("RGB",(grain,grain,grain)).point(lambda v:v*0.25),0.35)
d=ImageDraw.Draw(img)
hy=int(W*0.30)            # horizon
cx=W//2
def road(xt,xb): return [(cx-xt,hy),(cx+xt,hy),(cx+xb,W),(cx-xb,W)]
d.polygon(road(int(W*0.035),int(W*0.62)),fill=(46,46,46))
# white edge lines
for side in (-1,1):
    for t0,t1 in [(0.0,1.0)]:
        pts=[]
        for xt,xb,y in [(0.030,0.56,0)]:
            pass
    d.polygon([(cx+side*int(W*0.028),hy),(cx+side*int(W*0.033),hy),
               (cx+side*int(W*0.56),W),(cx+side*int(W*0.50),W)],fill=(235,235,235))
# amber center dashes in perspective
def xy(f,half):  # f: 0 horizon .. 1 bottom
    y=hy+(W-hy)*f; w=half*(0.04+0.96*f); return y,w
f=0.02; i=0
while f<1.0:
    L=0.06+0.16*f; g=0.05+0.12*f
    y0,w0=xy(f,int(W*0.022)); y1,w1=xy(min(f+L,1.0),int(W*0.022))
    d.polygon([(cx-w0,y0),(cx+w0,y0),(cx+w1,y1),(cx-w1,y1)],fill=(224,161,0))
    f+=L+g
# soft horizon glow
glow=Image.new("L",(W,W),0); gd=ImageDraw.Draw(glow)
gd.ellipse([cx-int(W*0.5),hy-int(W*0.18),cx+int(W*0.5),hy+int(W*0.18)],fill=60)
glow=glow.filter(ImageFilter.GaussianBlur(W*0.06))
img=Image.composite(Image.new("RGB",(W,W),(224,161,0)),img,glow.point(lambda v:int(v*0.6)))
img=img.resize((N,N),Image.LANCZOS)
img.save(sys.argv[1]); img.resize((180,180),Image.LANCZOS).save(sys.argv[2]); img.resize((60,60),Image.LANCZOS).save(sys.argv[3])

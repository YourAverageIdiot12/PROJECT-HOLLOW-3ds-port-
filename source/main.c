// Project Hollow - Nintendo 3DS port (libctru + citro2d, software 3D with stereoscopic output)
#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define CC(r,g,b) ((u32)(r)|((u32)(g)<<8)|((u32)(b)<<16)|0xFF000000u)
#define U 20.0f          // pixels (game units) per 3D unit
#define MAXW 128
#define MAXQ 2600

static float cl(float v,float a,float b){return v<a?a:v>b?b:v;}
static float rnd(void){return rand()/(float)RAND_MAX;}
static float dist(float a,float b,float c,float d){return hypotf(a-c,b-d);}

/* ---------------- maps ---------------- */
typedef struct{float x,y,w,h;}Wall;
typedef struct{const char*name;float w,h;int nw,lob;Wall wl[MAXW];float gens[4][2],ex[2],sp[3][2],k[2];u32 fog,fa,fb,wc,pc;float fn,ff,pf;}Map;
static Map maps[3];static Map*M;
static void W(Map*m,float x,float y,float w,float h){if(m->nw<MAXW)m->wl[m->nw++]=(Wall){x,y,w,h};}
static void pts(Map*m,const float g[4][2],float ex,float ey,const float s[3][2],float kx,float ky){
 memcpy(m->gens,g,sizeof(float)*8);m->ex[0]=ex;m->ex[1]=ey;memcpy(m->sp,s,sizeof(float)*6);m->k[0]=kx;m->k[1]=ky;}
static void initMaps(void){
 // 0: Hollow Facility - cold, flickering, steel corridors
 Map*a=&maps[0];memset(a,0,sizeof*a);a->name="Hollow Facility";a->w=1600;a->h=1000;
 a->fog=CC(5,12,14);a->fa=CC(26,29,34);a->fb=CC(33,38,44);a->wc=CC(58,76,88);a->pc=CC(170,255,238);a->fn=4;a->ff=20;a->pf=.15f;
 static const float fw[][4]={{380,0,30,330},{380,450,30,550},{800,200,30,400},{800,720,30,280},{1180,0,30,420},{1180,540,30,460},{100,500,200,30},{1210,500,250,30},{500,300,200,30},{900,420,200,30}};
 for(int i=0;i<10;i++)W(a,fw[i][0],fw[i][1],fw[i][2],fw[i][3]);
 static const float g0[4][2]={{200,250},{600,800},{1000,150},{1400,750}},s0[3][2]={{90,90},{90,200},{90,310}};
 pts(a,g0,1520,500,s0,1450,100);
 // 1: Ashen Woods - dusky red, embers, trees
 Map*b=&maps[1];memset(b,0,sizeof*b);b->name="Ashen Woods";b->w=1600;b->h=1000;
 b->fog=CC(42,14,8);b->fa=CC(30,21,17);b->fb=CC(38,27,21);b->wc=CC(74,58,42);b->pc=CC(255,122,48);b->fn=3;b->ff=16;b->pf=1.6f;
 static const float g1[4][2]={{300,200},{800,880},{1300,300},{700,470}},s1[3][2]={{90,900},{90,800},{190,900}};
 pts(b,g1,1520,900,s1,1500,100);
 {unsigned long long s=7;float P[9][2];memcpy(P,g1,sizeof g1);P[4][0]=1520;P[4][1]=900;memcpy(P[5],s1,sizeof s1);P[8][0]=1500;P[8][1]=100;
  #define R01() (s=(s*16807ULL)%2147483647ULL,s/2147483647.0f)
  for(int i=0;i<90;i++){float x=R01()*1500+50,y=R01()*900+50,z=34+R01()*30;int bad=0;for(int j=0;j<9;j++)if(dist(P[j][0],P[j][1],x,y)<110)bad=1;if(!bad)W(b,x-z/2,y-z/2,z,z);}
  W(b,560,200,40,220);W(b,1000,600,260,40);W(b,200,600,40,200);}
 // 2: Lobby (gens[] hold the two vote pads)
 Map*c=&maps[2];memset(c,0,sizeof*c);c->name="Lobby";c->w=700;c->h=450;c->lob=1;
 c->fog=CC(26,22,48);c->fa=CC(36,40,68);c->fb=CC(44,48,80);c->wc=CC(74,80,128);c->pc=CC(255,255,255);c->fn=25;c->ff=70;c->pf=.05f;
 W(c,250,150,40,40);W(c,410,260,40,40);
 static const float g2[4][2]={{150,225},{550,225},{0,0},{0,0}},s2[3][2]={{300,225},{340,200},{340,260}};
 pts(c,g2,0,0,s2,0,0);
}

/* ---------------- game state ---------------- */
typedef struct{float x,y,r,hp,st,ang,boost,wx,wy,wt,ph,mvs,cd,dash,dcd,sl,stk,side,px,py,bcd;int alive,esc,cola,type;}Ent;
typedef struct{float x,y,p;}Gen;
typedef struct{float x,y,t;int hit,on;}Blk;
enum{LOBBY,PLAY,RESULT};
static Ent sv[3],K;static Gen gn[4];static Blk bl[8];
static int phase,round_,wins,losses,cls,vote=-1,pk,open_,win_,kb;
static float tmr,timeL,grace,camy,ch,chs,tt,ix_,iy_,il_,parts[80][3];
static char why[64];static u32 held,down;

static void col(Ent*e){
 for(int i=0;i<M->nw;i++){Wall*w=&M->wl[i];float cx=cl(e->x,w->x,w->x+w->w),cy=cl(e->y,w->y,w->y+w->h),dx=e->x-cx,dy=e->y-cy,d=sqrtf(dx*dx+dy*dy);
  if(d<e->r){if(d<.001f)e->y=w->y-e->r;else{e->x+=dx/d*(e->r-d);e->y+=dy/d*(e->r-d);}}}
 e->x=cl(e->x,e->r,M->w-e->r);e->y=cl(e->y,e->r,M->h-e->r);}
static void mv(Ent*e,float vx,float vy,float dt){float x0=e->x,y0=e->y;if(vx!=0||vy!=0)e->ang=atan2f(vx,vy);e->x+=vx;col(e);e->y+=vy;col(e);
 e->mvs=dist(e->x,e->y,x0,y0)/dt/U;e->ph+=e->mvs*dt*1.6f;}
static float stepTo(Ent*e,float tx,float ty,float sp,float dt){float dx=tx-e->x,dy=ty-e->y,d=sqrtf(dx*dx+dy*dy);if(d<1)return 1;
 float x0=e->x,y0=e->y;mv(e,dx/d*sp*dt,dy/d*sp*dt,dt);return dist(e->x,e->y,x0,y0)/(sp*dt);}
static void cola(Ent*e){if(e->type==1&&e->cola>0&&e->boost<=0&&phase!=RESULT){e->cola--;e->boost=4;}}
static void dropBlocks(Ent*tg){for(int i=0;i<4;i++){int k=-1;for(int j=0;j<8;j++)if(!bl[j].on){k=j;break;}if(k<0)break;
 float a=rnd()*6.28f,r=i?80+rnd()*90:0;bl[k]=(Blk){cl(tg->x+cosf(a)*r,40,M->w-40),cl(tg->y+sinf(a)*r,40,M->h-40),-1.3f,0,1};}}
static void place(Map*m){
 for(int i=0;i<3;i++){Ent*e=&sv[i];memset(e,0,sizeof*e);e->x=m->sp[i][0];e->y=m->sp[i][1];e->hp=100;e->st=100;e->r=12;e->alive=1;e->cola=2;e->type=(i==1)?1:0;}
 sv[0].type=cls;memset(&K,0,sizeof K);K.x=m->k[0];K.y=m->k[1];K.r=15;K.dcd=6;K.bcd=7;
 for(int i=0;i<4;i++)gn[i]=(Gen){m->gens[i][0],m->gens[i][1],0};memset(bl,0,sizeof bl);
 for(int i=0;i<80;i++){parts[i][0]=rnd()*m->w/U;parts[i][1]=rnd()*12;parts[i][2]=rnd()*m->h/U;}}
static void goLobby(void){pk=0;phase=LOBBY;tmr=15;vote=-1;M=&maps[2];place(M);}
static void startPlay(void){pk=rnd()<.35f;int mi=vote>=0?vote:round_%2;round_++;M=&maps[mi];place(M);open_=0;timeL=180;grace=3;phase=PLAY;}
static void endRound(int w){phase=RESULT;tmr=4;win_=w;if(w)wins++;else losses++;}
static Gen*nearGen(float x,float y,float r){for(int i=0;i<4;i++)if(gn[i].p<100&&dist(gn[i].x,gn[i].y,x,y)<r)return &gn[i];return NULL;}

static void bot(Ent*b,int lob,float dt){
 if(!b->alive||b->esc)return;float mul=b->boost>0?1.7f:1;
 if(lob){b->wt-=dt;if(b->wt<=0){b->wt=2+rnd()*2;b->wx=80+rnd()*540;b->wy=80+rnd()*290;}stepTo(b,b->wx,b->wy,70,dt);return;}
 float kd=dist(K.x,K.y,b->x,b->y);
 if(kd<200){cola(b);stepTo(b,b->x+(b->x-K.x),b->y+(b->y-K.y),125*mul,dt);return;}
 if(open_){stepTo(b,M->ex[0],M->ex[1],115*mul,dt);if(dist(M->ex[0],M->ex[1],b->x,b->y)<40)b->esc=1;return;}
 Gen*g=NULL;float bd=1e9f;for(int i=0;i<4;i++)if(gn[i].p<100){float d=dist(gn[i].x,gn[i].y,b->x,b->y);if(d<bd){bd=d;g=&gn[i];}}
 if(g){if(bd<42)g->p=fminf(100,g->p+9*dt);else stepTo(b,g->x,g->y,110*mul,dt);}}

static void readMove(void){
 circlePosition cp;hidCircleRead(&cp);float ix=cp.dx/140.f,iy=-cp.dy/140.f;if(fabsf(ix)<.12f)ix=0;if(fabsf(iy)<.12f)iy=0;
 if(held&KEY_DLEFT)ix-=1;if(held&KEY_DRIGHT)ix+=1;if(held&KEY_DUP)iy-=1;if(held&KEY_DDOWN)iy+=1;
 float l=sqrtf(ix*ix+iy*iy);if(l>1){ix/=l;iy/=l;l=1;}ix_=ix;iy_=iy;il_=l;}

static void update(float dt){
 ch=0;tt+=dt;
 for(int i=0;i<80;i++){parts[i][1]-=M->pf*dt;parts[i][0]+=sinf(tt+i)*dt*(M->pf>1?.8f:.4f);if(parts[i][1]<0)parts[i][1]=12;}
 if(held&KEY_L)camy+=2*dt;if(held&KEY_R)camy-=2*dt;
 if(phase==RESULT){tmr-=dt;if(tmr<=0)goLobby();return;}
 int lob=phase==LOBBY;Ent*P=&sv[0];readMove();
 if(down&KEY_X){if(pk&&!lob)kb=1;else cola(P);}
 if(lob){tmr-=dt;if(tmr<=0){startPlay();return;}if(down&KEY_Y)cls^=1;P->type=cls;
  for(int i=0;i<2;i++)if(dist(P->x,P->y,M->gens[i][0],M->gens[i][1])<60)vote=i;}
 else{timeL-=dt;if(grace>0)grace-=dt;}
 for(int i=0;i<3;i++)sv[i].boost=fmaxf(0,sv[i].boost-dt);
 if(P->alive&&!P->esc&&(!pk||lob)){
  int run=(held&KEY_B)&&P->st>0&&il_>.1f;float mul=P->boost>0?1.7f:1;
  if(run)P->st=fmaxf(0,P->st-35*dt);else P->st=fminf(100,P->st+18*dt);
  float sp=(run?175:120)*mul,c=cosf(camy),s=sinf(camy);
  mv(P,(c*ix_+s*iy_)*sp*dt,(-s*ix_+c*iy_)*sp*dt,dt);
  if(!lob){if(held&KEY_A){Gen*g=nearGen(P->x,P->y,48);if(g)g->p=fminf(100,g->p+14*dt);}
   if(open_&&dist(M->ex[0],M->ex[1],P->x,P->y)<40)P->esc=1;}}
 for(int i=(pk&&!lob)?0:1;i<3;i++)bot(&sv[i],lob,dt);
 if(lob)return;
 if(!open_){int all=1;for(int i=0;i<4;i++)if(gn[i].p<100)all=0;if(all)open_=1;}
 // killer
 int ti=-1;float bd=1e9f;for(int i=0;i<3;i++)if(sv[i].alive&&!sv[i].esc){float d=dist(sv[i].x,sv[i].y,K.x,K.y);if(d<bd){bd=d;ti=i;}}
 if(ti>=0&&grace<=0){Ent*tg=&sv[ti];float d=bd;K.cd-=dt;K.dcd-=dt;K.sl-=dt;K.bcd-=dt;
  float sp=(128+6*(round_-1))*(K.sl>0?.55f:1);
  if(K.dcd<=0&&(pk?(held&KEY_B)!=0:d<280)){K.dash=.45f;K.dcd=pk?6:8;}
  if(K.dash>0){K.dash-=dt;sp*=2;}
  if(K.bcd<=0&&(pk?kb:d<520)){dropBlocks(tg);K.bcd=pk?8:10;}
  if(pk){float c=cosf(camy),s=sinf(camy);mv(&K,(c*ix_+s*iy_)*sp*dt,(-s*ix_+c*iy_)*sp*dt,dt);}
  else{float tx=tg->x,ty=tg->y;if(K.side>0){K.side-=dt;tx=K.x+K.px*100;ty=K.y+K.py*100;}
   float r=stepTo(&K,tx,ty,sp,dt);K.stk=r<.4f?K.stk+dt:0;
   if(K.stk>.4f){K.stk=0;K.side=.7f;float dx=tg->x-K.x,dy=tg->y-K.y,dd=sqrtf(dx*dx+dy*dy);if(dd<1)dd=1;float sg=rnd()<.5f?1:-1;K.px=-dy/dd*sg;K.py=dx/dd*sg;}}
  if(d<K.r+tg->r+6&&K.cd<=0){tg->hp-=34;K.cd=1.2f;K.sl=.8f;if(tg->hp<=0)tg->alive=0;}}
 kb=0;
 {float d2=pk?bd:dist(K.x,K.y,P->x,P->y);if(grace<=0&&d2<340&&(pk?ti>=0:(P->alive&&!P->esc)))ch=cl(1.25f-d2/340,.35f,1);}
 for(int j=0;j<8;j++)if(bl[j].on){Blk*b=&bl[j];b->t+=dt;
  if(!b->hit&&b->t>=.3f){b->hit=1;for(int i=0;i<3;i++)if(sv[i].alive&&!sv[i].esc&&dist(sv[i].x,sv[i].y,b->x,b->y)<58){sv[i].hp-=30;if(sv[i].hp<=0)sv[i].alive=0;}}
  if(b->t>2.5f)b->on=0;}
 if(pk){int dead=0,act=0;for(int i=0;i<3;i++){if(!sv[i].alive)dead++;if(sv[i].alive&&!sv[i].esc)act++;}
  if(!act||timeL<=0){snprintf(why,sizeof why,"%d of 3 survivors eliminated",dead);endRound(dead>=2);}return;}
 if(P->esc){strcpy(why,"You escaped!");endRound(1);}
 else if(!P->alive){strcpy(why,"You were eliminated");endRound(0);}
 else if(timeL<=0){strcpy(why,"Time ran out");endRound(0);}
}

/* ---------------- software 3D renderer ---------------- */
typedef struct{float x[4],y[4],d;u32 c;}Q;
static Q qs[MAXQ];static int nq,eye;static float slider,Ex,Ey,Ez,Fx,Fy,Fz,Rx,Ry,Rz,Ux,Uy,Uz,gbright;static u32 fogc;
static int proj(float x,float y,float z,float*sx,float*sy,float*zc){
 float dx=x-Ex,dy=y-Ey,dz=z-Ez,zz=dx*Fx+dy*Fy+dz*Fz;if(zz<.4f)return 0;
 *sx=200+(dx*Rx+dy*Ry+dz*Rz)/zz*300+eye*slider*5*(1-8/zz);*sy=120-(dx*Ux+dy*Uy+dz*Uz)/zz*300;*zc=zz;return 1;}
static u32 shade(u32 c,float b,float z){
 float f=cl((z-M->fn)/(M->ff-M->fn),0,1),r=(c&255)*b,g=((c>>8)&255)*b,bb=((c>>16)&255)*b;
 r+=((fogc&255)-r)*f;g+=(((fogc>>8)&255)-g)*f;bb+=(((fogc>>16)&255)-bb)*f;
 return CC((int)cl(r,0,255),(int)cl(g,0,255),(int)cl(bb,0,255));}
static void pushQ(float*X,float*Y,float d,u32 c){if(nq>=MAXQ)return;Q*q=&qs[nq++];memcpy(q->x,X,16);memcpy(q->y,Y,16);q->d=d;q->c=c;}
static const int FI[5][4]={{1,3,7,5},{0,4,6,2},{2,3,7,6},{4,5,7,6},{0,2,3,1}};
static const float FN[5][3]={{1,0,0},{-1,0,0},{0,1,0},{0,0,1},{0,0,-1}};
// box centred at (cx,cy,cz) relative to pivot; rotated about X by ax around the pivot, then yawed and placed at (ex,ez)
static void addPart(float ex,float ez,float yw,float pvx,float pvy,float pvz,float ax,float cx,float cy,float cz,float w,float h,float d,float s,u32 col){
 float V[8][3],ca=cosf(ax),sa=sinf(ax),cw=cosf(yw),sw=sinf(yw);
 for(int i=0;i<8;i++){float x=cx+((i&1)?w:-w)/2,y=cy+((i&2)?h:-h)/2,z=cz+((i&4)?d:-d)/2;
  float y1=y*ca-z*sa,z1=y*sa+z*ca;x=(x+pvx)*s;y1=(y1+pvy)*s;z1=(z1+pvz)*s;
  V[i][0]=ex+x*cw+z1*sw;V[i][1]=y1;V[i][2]=ez-x*sw+z1*cw;}
 for(int fi=0;fi<5;fi++){float nx=FN[fi][0],ny=FN[fi][1]*ca-FN[fi][2]*sa,nz0=FN[fi][1]*sa+FN[fi][2]*ca;
  float nxr=nx*cw+nz0*sw,nzr=-nx*sw+nz0*cw,mx=0,my=0,mz=0;
  for(int k=0;k<4;k++){mx+=V[FI[fi][k]][0]/4;my+=V[FI[fi][k]][1]/4;mz+=V[FI[fi][k]][2]/4;}
  if(nxr*(Ex-mx)+ny*(Ey-my)+nzr*(Ez-mz)<=0)continue;
  float X[4],Y[4],zs=0,zc;int ok=1;
  for(int k=0;k<4;k++){const float*v=V[FI[fi][k]];if(!proj(v[0],v[1],v[2],&X[k],&Y[k],&zc)){ok=0;break;}zs+=zc/4;}
  if(!ok)continue;
  float b=(.35f+.65f*fmaxf(0,nxr*.35f+ny*.8f+nzr*.5f))*gbright;pushQ(X,Y,zs,shade(col,b,zs));}}
static void flat(float x0,float z0,float x1,float z1,float y,u32 c,float bias,float b){
 float X[4],Y[4],zs=0,zc;const float px[4]={x0,x1,x1,x0},pz[4]={z0,z0,z1,z1};
 for(int k=0;k<4;k++){if(!proj(px[k],y,pz[k],&X[k],&Y[k],&zc))return;zs+=zc/4;}pushQ(X,Y,zs+bias,shade(c,b*gbright,zs));}
static int cmpq(const void*a,const void*b){float d=((const Q*)b)->d-((const Q*)a)->d;return d>0?1:d<0?-1:0;}
static void chr(Ent*e,u32 tc,u32 sk,u32 lg,float s,int raise,int kill){
 float ex=e->x/U,ez=e->y/U,yw=e->ang,a=sinf(e->ph)*cl(e->mvs/2.5f,0,1)*.9f;
 addPart(ex,ez,yw,-.22f,1,0,a,0,-.5f,0,.42f,1,.42f,s,lg);addPart(ex,ez,yw,.22f,1,0,-a,0,-.5f,0,.42f,1,.42f,s,lg);
 addPart(ex,ez,yw,0,0,0,0,0,1.5f,0,.9f,1,.45f,s,tc);
 addPart(ex,ez,yw,-.66f,2,0,raise?-1.5f:-a,0,-.5f,0,.38f,1,.38f,s,sk);addPart(ex,ez,yw,.66f,2,0,raise?-1.5f:a,0,-.5f,0,.38f,1,.38f,s,sk);
 addPart(ex,ez,yw,0,0,0,0,0,2.3f,0,.6f,.6f,.6f,s,sk);
 if(kill){addPart(ex,ez,yw,0,0,0,0,-.14f,2.35f,.31f,.1f,.1f,.05f,s,CC(255,30,30));addPart(ex,ez,yw,0,0,0,0,.14f,2.35f,.31f,.1f,.1f,.05f,s,CC(255,30,30));}}

static C2D_TextBuf tb;
static void txt(const char*s,float x,float y,float sc,u32 c,int ctr){C2D_Text t;C2D_TextParse(&t,tb,s);C2D_TextOptimize(&t);C2D_DrawText(&t,C2D_WithColor|(ctr?C2D_AlignCenter:0),x,y,0,sc,sc,c);}

static void drawScene(C3D_RenderTarget*tgt,int e){
 eye=e;Ent*c=(pk&&phase!=LOBBY)?&K:&sv[0];
 float tx=c->x/U,tz=c->y/U;Ex=tx+sinf(camy)*7;Ey=6.5f;Ez=tz+cosf(camy)*7;
 float fx=tx-Ex,fy=1-Ey,fz=tz-Ez,l=sqrtf(fx*fx+fy*fy+fz*fz);Fx=fx/l;Fy=fy/l;Fz=fz/l;
 Rx=-Fz;Ry=0;Rz=Fx;l=sqrtf(Rx*Rx+Rz*Rz);Rx/=l;Rz/=l;Ux=Ry*Fz-Rz*Fy;Uy=Rz*Fx-Rx*Fz;Uz=Rx*Fy-Ry*Fx;
 gbright=(M==&maps[0]&&sinf(tt*40)>.9f&&sinf(tt*3)>.5f)?.3f:1;
 float cf=ch*.6f;fogc=CC((int)((M->fog&255)*(1-cf)+70*cf),(int)(((M->fog>>8)&255)*(1-cf)+6*cf),(int)(((M->fog>>16)&255)*(1-cf)+6*cf));
 C2D_TargetClear(tgt,fogc);C2D_SceneBegin(tgt);nq=0;
 // floor tiles
 {int gx=(int)floorf(tx/4),gz=(int)floorf(tz/4);for(int i=-5;i<=5;i++)for(int j=-5;j<=5;j++){float x0=(gx+i)*4.f,z0=(gz+j)*4.f;
  if(x0<0||z0<0||x0+4>M->w/U||z0+4>M->h/U)continue;flat(x0,z0,x0+4,z0+4,0,((gx+i+gz+j)&1)?M->fb:M->fa,500,1);}}
 float wh=M->lob?1.5f:3;
 for(int i=0;i<M->nw;i++){Wall*w=&M->wl[i];float nx=cl(tx*U,w->x,w->x+w->w),nz=cl(tz*U,w->y,w->y+w->h);if(dist(nx,nz,tx*U,tz*U)/U>M->ff+3)continue;
  addPart((w->x+w->w/2)/U,(w->y+w->h/2)/U,0,0,wh/2,0,0,0,0,0,w->w/U,wh,w->h/U,1,M->wc);}
 if(M->lob)for(int i=0;i<2;i++)flat(M->gens[i][0]/U-2.5f,M->gens[i][1]/U-2.5f,M->gens[i][0]/U+2.5f,M->gens[i][1]/U+2.5f,.05f,i?CC(255,138,42):CC(74,255,138),400,1.3f);
 else{
  for(int i=0;i<4;i++){float gx=gn[i].x/U,gz=gn[i].y/U;int dn=gn[i].p>=100;
   addPart(gx,gz,0,0,.55f,0,0,0,0,0,1.3f,1.1f,1.3f,1,dn?CC(47,209,106):CC(201,162,39));
   addPart(gx-.7f+.7f*gn[i].p/100,gz,0,0,1.5f,0,0,0,0,0,fmaxf(.02f,1.4f*gn[i].p/100),.18f,.18f,1,CC(255,211,77));}
  addPart(M->ex[0]/U,M->ex[1]/U,0,0,1.5f,0,0,0,0,0,1.8f,3,.4f,1,open_?CC(47,209,106):CC(107,28,28));
  for(int j=0;j<8;j++)if(bl[j].on){Blk*b=&bl[j];float bx=b->x/U,bz=b->y/U;
   if(b->t<0)flat(bx-2.2f,bz-2.2f,bx+2.2f,bz+2.2f,.06f,CC(255,40,40),300,1.2f);
   else addPart(bx,bz,0,0,1.3f+14*(1-fminf(1,b->t/.3f)),0,0,0,0,0,2.6f,2.6f,2.6f,1,CC(138,90,58));}}
 for(int i=0;i<3;i++)if(sv[i].alive&&!sv[i].esc){Ent*s=&sv[i];u32 tc=s->type?CC(255,138,42):(i==0?CC(111,208,255):CC(157,255,138));
  if(s->boost>0)tc=CC(255,190,80);
  chr(s,tc,i==2?CC(224,160,112):CC(245,205,48),i==1?CC(58,58,58):CC(42,74,154),1,0,0);}
 if(phase!=LOBBY)chr(&K,K.dash>0?CC(120,20,10):CC(26,26,26),CC(207,207,207),CC(10,10,10),1.4f,1,1);
 qsort(qs,nq,sizeof(Q),cmpq);
 for(int i=0;i<nq;i++){Q*q=&qs[i];C2D_DrawTriangle(q->x[0],q->y[0],q->c,q->x[1],q->y[1],q->c,q->x[2],q->y[2],q->c,0);
  C2D_DrawTriangle(q->x[0],q->y[0],q->c,q->x[2],q->y[2],q->c,q->x[3],q->y[3],q->c,0);}
 for(int i=0;i<80;i++){float sx,sy,zc;if(proj(parts[i][0],parts[i][1],parts[i][2],&sx,&sy,&zc)&&zc<M->ff){float z=fmaxf(1,.18f*300/zc);C2D_DrawRectSolid(sx,sy,0,z,z,shade(M->pc,1,zc));}}
 // HUD
 char b[96];tmr=tmr;int tm=(int)fmaxf(0,ceilf(timeL));
 if(phase!=LOBBY){snprintf(b,sizeof b,"R%d  %d:%02d  Gens %d/4",round_,tm/60,tm%60,(int)(gn[0].p>=100)+(gn[1].p>=100)+(gn[2].p>=100)+(gn[3].p>=100));txt(b,6,4,.5f,CC(232,213,213),0);}
 if(phase==LOBBY){snprintf(b,sizeof b,"LOBBY  %d",(int)ceilf(tmr));txt(b,200,90,.9f,CC(255,255,255),1);}
 else if(phase==PLAY&&grace>0){snprintf(b,sizeof b,"ROUND %d - %s%s",round_,M->name,pk?"\nYOU ARE THE KILLER":"");txt(b,200,90,.7f,CC(255,255,255),1);}
 else if(phase==RESULT){snprintf(b,sizeof b,"%s\n%s",win_?"ROUND WON":"ROUND LOST",why);txt(b,200,90,.8f,win_?CC(95,229,138):CC(255,90,90),1);}
}

/* ---------------- chase theme (ndsp) ---------------- */
#define SR 16000
#define NS (SR*2)
static s16*pcm;static ndspWaveBuf wb;static int audioOK;
static void setVol(float v){float m[12]={0};m[0]=m[1]=v;ndspChnSetMix(0,m);}
static void audioInit(void){
 if(R_FAILED(ndspInit()))return;
 static const float B[16]={55,0,55,0,65.4f,0,55,0,73.4f,0,55,0,77.8f,0,82.4f,0},L[16]={220,0,261.6f,311.1f,0,261.6f,0,329.6f,311.1f,0,261.6f,220,0,233.1f,246.9f,0};
 pcm=(s16*)linearAlloc(NS*2);if(!pcm)return;
 for(int i=0;i<16;i++)for(int n=0;n<SR/8;n++){float t=(float)n/SR,v=0;
  if(B[i]>0)v+=.5f*(fmodf(B[i]*t,1)*2-1)*expf(-t*8);if(i%4==0)v+=.9f*sinf(6.283f*48*t)*expf(-t*20);
  if(L[i]>0)v+=.12f*(fmodf(L[i]*t,1)<.5f?1:-1)*expf(-t*10);pcm[i*(SR/8)+n]=(s16)(cl(v,-1,1)*12000);}
 ndspSetOutputMode(NDSP_OUTPUT_STEREO);ndspChnReset(0);ndspChnSetInterp(0,NDSP_INTERP_LINEAR);ndspChnSetRate(0,SR);ndspChnSetFormat(0,NDSP_FORMAT_MONO_PCM16);
 memset(&wb,0,sizeof wb);wb.data_vaddr=pcm;wb.nsamples=NS;wb.looping=true;DSP_FlushDataCache(pcm,NS*2);ndspChnWaveBufAdd(0,&wb);setVol(0);audioOK=1;}

int main(int argc,char**argv){
 (void)argc;(void)argv;
 gfxInitDefault();gfxSet3D(true);consoleInit(GFX_BOTTOM,NULL);
 C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);C2D_Init(12000);C2D_Prepare();
 C3D_RenderTarget*tl=C2D_CreateScreenTarget(GFX_TOP,GFX_LEFT),*tr=C2D_CreateScreenTarget(GFX_TOP,GFX_RIGHT);
 tb=C2D_TextBufNew(4096);srand((unsigned)osGetTime());initMaps();audioInit();goLobby();
 u64 last=osGetTime();int fr=0;
 while(aptMainLoop()){
  hidScanInput();held=hidKeysHeld();down=hidKeysDown();if(down&KEY_START)break;
  u64 now=osGetTime();float dt=cl((now-last)/1000.0f,0.001f,.05f);last=now;
  update(dt);chs+=(ch-chs)*.1f;if(audioOK)setVol(chs*.5f);
  slider=osGet3DSliderState();
  C3D_FrameBegin(C3D_FRAME_SYNCDRAW);C2D_TextBufClear(tb);
  drawScene(tl,-1);if(slider>0.02f)drawScene(tr,1);
  C3D_FrameEnd(0);
  if(fr++%6==0){Ent*P=&sv[0];consoleClear();
   printf("PROJECT HOLLOW\n\nRound %d   W%d L%d\nMap: %s\n",round_,wins,losses,M->name);
   if(phase==LOBBY)printf("\nLOBBY - next round in %d s\nStand on the green/orange pad to vote\nVote: %s\nClass: %s (Y to switch)\n35%% chance to play Killer\n",(int)ceilf(tmr),vote<0?"auto":maps[vote].name,cls?"Cola Kid":"Runner");
   else if(pk)printf("\nYOU ARE THE KILLER\nDash (B): %s\nBlocks (X): %s\nEliminate 2+ survivors\n",K.dcd>0?"wait":"READY",K.bcd>0?"wait":"READY");
   else printf("\nHP %d  Stamina %d\n%s\n%s\n",(int)P->hp,(int)P->st,P->type?(P->cola?"Bloxy Cola ready (X)":"Out of cola"):"Runner",open_?"EXIT OPEN - RUN!":"Repair all generators (A)");
   printf("\nCircle Pad move  B run/dash\nA repair  X cola/blocks\nL/R camera   START exit\n3D slider = depth\n");}
 }
 if(audioOK){ndspChnReset(0);linearFree(pcm);ndspExit();}
 C2D_TextBufDelete(tb);C2D_Fini();C3D_Fini();gfxExit();return 0;}

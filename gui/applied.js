/* ================= APPLIED : A SECTOR RADAR YOU CAN OPERATE =================
   Everything above this point is a reconstruction of somebody else's console.
   This one works. Every control is one of the patterns named earlier on the page,
   wired up for real: the buttons move the picture, dragging measures it, the
   tripwire raises an alarm that has to be held to acknowledge, and the fault
   switch takes the timing reference away so the display has to admit what it no
   longer knows. */
(function(){
  var cv=document.getElementById("applied"); if(!cv) return;
  var ctx=cv.getContext("2d"), W=cv.width, H=cv.height;

  /* ---------- geometry ---------- */
  var CTRL_Y=H-176, HEALTH_Y=H-112, GUIDE_Y=H-40;
  var SP ={x:0,  y:28, w:648, h:CTRL_Y-28};
  var RD ={x:660,y:28, w:286, h:300};
  var MD ={x:660,y:340,w:286, h:236};
  var AUX={x:660,y:588,w:286, h:CTRL_Y-588-4};
  var TL ={x:958,y:28, w:W-958-14, h:CTRL_Y-28};
  var HALF=Math.PI/4;                       /* the array covers about ±45° */
  var cx=SP.w/2, cy=CTRL_Y-46, R=(SP.w/2-46)/Math.sin(HALF);

  var col={drone:C.red,suspect:C.orange,bird:C.green,unk:C.grey};
  var CLSNAME={drone:"DRONE",suspect:"SUSPECT",bird:"BIRD",unk:"UNCLASSIFIED"};

  /* ---------- what the operator has set ---------- */
  var RANGES=[0.3,0.6,1.2,2.4];
  var st={
    rangeKm:1.2, rangeFine:false,
    gain:0.62, gainAuto:false,
    video:1,            /* 0 raw only · 1 raw plus what moved */
    decay:0,            /* 0 smooth · 1 stepped · 2 none */
    show:0,             /* 0 everything · 1 no video · 2 tracks only */
    listen:false,
    sel:0,
    ebl:null,           /* the bearing line and range ring, dragged by hand */
    cue:null,           /* a look the operator has asked for but not yet had */
    alarm:"none",       /* none · unack · ack */
    fault:false, dropped:0,
    hold:null,          /* {kind, t0} — guarded actions are held, not confirmed */
    drag:null, hover:null, hoverTrk:-1, focusKb:false
  };

  /* ---------- the tracks ---------- */
  var nextId=22;
  function mkTrack(id,cls,az,rg,vaz,mps,alt,cf){
    return {id:id,cls:cls,az:az,rg:rg,vaz:vaz,mps:mps,alt:alt,cf:cf,
            state:"live",coast:0,seed:(id.charCodeAt(3)*97+id.charCodeAt(4)*31)%1999+7};
  }
  var trk=[
    mkTrack("TK-014","drone",  -0.31,0.62, 0.011,-14.2,84,0.94),
    mkTrack("TK-017","suspect", 0.10,0.76, 0.017, -9.6,51,0.61),
    mkTrack("TK-011","bird",    0.55,0.78,-0.021, -6.1,37,0.88),
    mkTrack("TK-021","unk",    -0.62,0.35, 0.007, -3.4,12,null)
  ];

  /* A tripwire the operator owns: bearings and ranges, dragged about by hand. */
  var guard={az0:-0.06,az1:0.30,r0:0.32,r1:0.58};

  /* The blind region behind a roofline. Computed from a height profile rather
     than drawn by hand, so it is jagged where the roof is jagged. */
  var shadow=(function(){
    var r=rnd(613), pts=[], a, i, n=26, a0=0.30, a1=0.66, h=0.52;
    for(i=0;i<=n;i++){
      a=a0+(a1-a0)*i/n;
      h=h*0.72+ (0.40+r()*0.30)*0.28;
      pts.push([a,h]);
    }
    return {a0:a0,a1:a1,pts:pts};
  })();

  /* Stationary clutter: the hedge, the shed roof, the line of parked cars. Fixed
     seed, so it is in the same place every frame — which is the whole point of
     subtracting a running average from it. */
  var clutter=(function(){
    var r=rnd(4409), out=[], i;
    for(i=0;i<520;i++){
      var a=(r()*2-1)*HALF*0.99, rg=0.07+r()*0.92;
      out.push({az:a,rg:rg,amp:0.20+r()*0.80});
    }
    return out;
  })();

  /* ---------- the rolling speed signature ---------- */
  var mdBuf=document.createElement("canvas");
  mdBuf.width=MD.w-2; mdBuf.height=MD.h-56;
  var mdCtx=mdBuf.getContext("2d");
  var mdMid=Math.round(mdBuf.height*0.5);
  var mdCls=null;
  function mdWipe(){ mdCtx.fillStyle="#050807"; mdCtx.fillRect(0,0,mdBuf.width,mdBuf.height); }
  mdWipe();
  /* One column of trace. A body moving as a whole writes the hard centre line;
     anything spinning on it writes a band either side, and how far out that band
     sits is the blade tip speed. A wingbeat wobbles the line itself instead. */
  function mdColumn(t,cls){
    mdCtx.drawImage(mdBuf,-1,0);
    mdCtx.fillStyle="#050807"; mdCtx.fillRect(mdBuf.width-1,0,1,mdBuf.height);
    var rr=rnd((Math.floor(t/16)%4000)+11), c=col[cls]||C.grey;
    var rgb = c===C.red?"255,19,32" : c===C.orange?"254,147,13" : c===C.green?"35,225,98" : "179,179,179";
    var wob = cls==="bird" ? Math.sin(t/165)*7+Math.sin(t/61)*2.5 : 0;
    var body = mdMid+wob;
    /* the airframe itself */
    if(cls!=="unk"){
      mdCtx.fillStyle="rgba(255,255,255,"+(cls==="bird"?0.55:0.9)+")";
      mdCtx.fillRect(mdBuf.width-1,body-1,1,cls==="bird"?2:3);
    }
    /* blade lines: a pair of bands, hard for a rotor, on and off for an unproven
       one, absent for a bird, and never there at all when the dwell was too short */
    var on = cls==="drone" ? 1
           : cls==="suspect" ? (Math.sin(t/900)>0.15?1:0)
           : 0;
    /* a blade is only bright while it is broadside on, so the band is written in
       flashes — the stripes down the trace are the blade passing frequency */
    var bflash = 0.12+0.88*Math.pow(Math.abs(Math.sin(t/118)),3);
    if(on){
      for(var k=-1;k<=1;k+=2){
        for(var d=26;d<46;d++){
          var y=body+k*d;
          if(y<0||y>=mdBuf.height) continue;
          var edge=1-Math.abs(d-36)/12;
          var a=0.62*edge*bflash*(0.4+0.6*rr());
          if(a>0.02){ mdCtx.fillStyle="rgba("+rgb+","+a.toFixed(3)+")"; mdCtx.fillRect(mdBuf.width-1,y,1,1); }
        }
      }
    }
    /* the soft near-in flutter every real target has */
    var spread = cls==="bird"?17 : cls==="unk"?9 : 7;
    for(var yy=-spread;yy<=spread;yy++){
      var a2=(0.26-Math.abs(yy)*0.011)*rr();
      if(a2>0.012){ mdCtx.fillStyle="rgba("+rgb+","+a2.toFixed(3)+")";
        mdCtx.fillRect(mdBuf.width-1,body+yy,1,1); }
    }
  }
  function mdWarm(cls){ mdWipe(); for(var i=0;i<mdBuf.width;i++) mdColumn(i*16,cls); mdCls=cls; }
  var MDCAP={
    drone:  ["BLADE LINES AT ±41 Hz  ·  ROTARY", C.red],
    suspect:["BLADE LINES INTERMITTENT  ·  UNRESOLVED", C.orange],
    bird:   ["WINGBEAT ONLY  ·  NO BLADE LINES", C.green],
    unk:    ["NO USABLE SIGNATURE  ·  DWELL TOO SHORT", C.grey]
  };

  /* ---------- the control bar ---------- */
  var CTRLS=[
    {k:"range", name:"RANGE"},
    {k:"gain",  name:"RX GAIN"},
    {k:"video", name:"VIDEO"},
    {k:"decay", name:"DECAY"},
    {k:"show",  name:"SHOW"},
    {k:"listen",name:"AUDIO"},
    {k:"ack",   name:"ALARM"},
    {k:"fault", name:"SIMULATE"}
  ];
  (function(){
    var pad=14, gap=10, w=Math.floor((W-pad*2-gap*(CTRLS.length-1))/CTRLS.length);
    CTRLS.forEach(function(c,i){ c.x=pad+i*(w+gap); c.y=CTRL_Y+24; c.w=w; c.h=34; });
  })();
  var VIDEONAME=["RAW ONLY","+ WHAT MOVED"], DECAYNAME=["SMOOTH 3 s","STEPPED ×4","NONE"],
      SHOWNAME=["EVERYTHING","NO VIDEO","TRACKS ONLY"];
  function ctrlValue(c){
    switch(c.k){
      case "range":  return st.rangeKm.toFixed(st.rangeFine?2:1)+" km";
      case "gain":   return (st.gain*100).toFixed(0)+" %";
      case "video":  return VIDEONAME[st.video];
      case "decay":  return DECAYNAME[st.decay];
      case "show":   return SHOWNAME[st.show];
      case "listen": return st.listen?("ON · "+sel().id):"OFF";
      case "ack":    return st.alarm==="none"?"NO ALARM":st.alarm==="unack"?"HOLD TO ACK":"ACKNOWLEDGED";
      case "fault":  return st.fault?"HOLD TO RESTORE":"HOLD: REF LOSS";
    }
  }
  var CTRLHELP={
    range:"LEFT step the range · WHEEL fine adjust the same control",
    gain:"DRAG to set the gain · CLICK the tag to hand it to the machine",
    video:"LEFT switch between the raw return and the raw return plus what moved",
    decay:"LEFT cycle how the past fades — seconds, sweeps, or not at all",
    show:"LEFT drop layers out of the picture · the tripwire comes back on its own if it fires",
    listen:"LEFT route the selected track's Doppler tone to the headset",
    ack:"HOLD to acknowledge · the alarm changes state, it does not disappear",
    fault:"HOLD to take the timing reference away and watch the display admit it"
  };
  function sel(){ return trk[Math.min(st.sel,trk.length-1)]||trk[0]; }

  /* ---------- input ---------- */
  function pos(e){
    var r=cv.getBoundingClientRect();
    return {x:(e.clientX-r.left)*(cv.width/r.width), y:(e.clientY-r.top)*(cv.height/r.height)};
  }
  function inBox(p,b){ return p.x>=b.x&&p.x<=b.x+b.w&&p.y>=b.y&&p.y<=b.y+b.h; }
  function inWedge(p){
    var dx=p.x-cx, dy=p.y-cy, d=Math.hypot(dx,dy);
    if(d>R||d<8) return null;
    var a=Math.atan2(dy,dx)+Math.PI/2;
    if(Math.abs(a)>HALF) return null;
    return {az:a, rg:d/R};
  }
  function hitCtrl(p){ for(var i=0;i<CTRLS.length;i++) if(inBox(p,CTRLS[i])) return CTRLS[i]; return null; }
  function hitTrack(p){
    for(var i=0;i<trk.length;i++){
      var g=trk[i];
      if(g.state==="dropped"||g._x===undefined) continue;
      if(Math.hypot(p.x-g._x,p.y-g._y)<15) return i;
    }
    return -1;
  }
  function hitList(p){
    if(p.x<TL.x-10||p.x>W) return -1;
    for(var i=0;i<trk.length;i++){ var y=TL.y+44+i*76; if(p.y>y-18&&p.y<y+52) return i; }
    return -1;
  }
  function inGuard(p){
    var q=inWedge(p); if(!q) return false;
    return q.az>guard.az0&&q.az<guard.az1&&q.rg>guard.r0&&q.rg<guard.r1;
  }
  function select(i){
    if(i<0||i>=trk.length||trk[i].state==="dropped") return;
    st.sel=i; mdWarm(trk[i].cls); repaint();
  }
  function stepRange(dir){
    var i=RANGES.indexOf(st.rangeKm);
    if(i<0){ /* came off a fine adjustment — snap to the nearest step first */
      i=0; for(var k=0;k<RANGES.length;k++) if(Math.abs(RANGES[k]-st.rangeKm)<Math.abs(RANGES[i]-st.rangeKm)) i=k;
      if((dir>0&&RANGES[i]<=st.rangeKm)||(dir<0&&RANGES[i]>=st.rangeKm)) i+=dir;
    } else i+=dir;
    st.rangeKm=RANGES[Math.max(0,Math.min(RANGES.length-1,i))]; st.rangeFine=false;
  }
  function startHold(kind){
    if(kind==="ack"&&st.alarm!=="unack") return;
    st.hold={kind:kind,t0:null,p:0};
  }
  function finishHold(kind){
    if(kind==="ack"){ st.alarm="ack"; }
    else if(kind==="fault"){
      if(!st.fault){
        st.fault=true;
        trk.forEach(function(g){ if(g.state==="live"){ g.state="coast"; g.coast=8; } });
      } else {
        st.fault=false;
        st.dropped+=trk.filter(function(g){return g.state!=="live";}).length;
        var r=rnd(nextId*17+3);
        trk=[
          mkTrack("TK-0"+(nextId++),"drone",  -0.20-r()*0.3, 0.70+r()*0.2,  0.011,-14.2,84,0.91),
          mkTrack("TK-0"+(nextId++),"suspect", 0.05+r()*0.2, 0.62+r()*0.2,  0.017, -9.6,51,0.58),
          mkTrack("TK-0"+(nextId++),"bird",    0.40+r()*0.3, 0.80+r()*0.15,-0.021, -6.1,37,0.86),
          mkTrack("TK-0"+(nextId++),"unk",    -0.55-r()*0.2, 0.55+r()*0.3,  0.007, -3.4,12,null)
        ];
        st.sel=0; st.alarm="none"; mdWarm(trk[0].cls);
      }
    }
  }
  cv.addEventListener("pointerdown",function(e){
    cv.focus(); st.focusKb=false;
    var p=pos(e);
    if(e.button===2) return;
    var c=hitCtrl(p);
    if(c){
      cv.setPointerCapture&&cv.setPointerCapture(e.pointerId);
      if(c.k==="range"){ stepRange(p.x < c.x+c.w/2 ? -1 : 1); }
      else if(c.k==="gain"){
        if(p.x>c.x+c.w-42){ st.gainAuto=!st.gainAuto; }
        else { st.gainAuto=false; st.gain=Math.max(0,Math.min(1,(p.x-c.x-8)/(c.w-52))); st.drag={k:"gain",c:c}; }
      }
      else if(c.k==="video"){ st.video=(st.video+1)%2; }
      else if(c.k==="decay"){ st.decay=(st.decay+1)%3; }
      else if(c.k==="show"){ st.show=(st.show+1)%3; }
      else if(c.k==="listen"){ st.listen=!st.listen; }
      else if(c.k==="ack"||c.k==="fault"){ startHold(c.k); }
      repaint(); return;
    }
    var li=hitList(p); if(li>=0){ select(li); return; }
    var ti=hitTrack(p); if(ti>=0){ select(ti); return; }
    var q=inWedge(p);
    if(q){
      cv.setPointerCapture&&cv.setPointerCapture(e.pointerId);
      if(inGuard(p)) st.drag={k:"guard",az:q.az,rg:q.rg};
      else { st.ebl={az:q.az,rg:q.rg}; st.drag={k:"ebl"}; }
      repaint();
    }
  });
  cv.addEventListener("pointermove",function(e){
    var p=pos(e);
    var q=inWedge(p);
    cv.style.cursor = q ? "crosshair" : (hitCtrl(p)||hitList(p)>=0 ? "pointer" : "default");
    var c=hitCtrl(p); var nh=c?c.k:(q?"picture":null);
    var nt=hitTrack(p);
    if(nh!==st.hover||nt!==st.hoverTrk){ st.hover=nh; st.hoverTrk=nt; repaint(); }
    if(!st.drag) return;
    if(st.drag.k==="gain"){
      st.gain=Math.max(0,Math.min(1,(p.x-st.drag.c.x-8)/(st.drag.c.w-52))); repaint();
    } else if(st.drag.k==="ebl"&&q){ st.ebl={az:q.az,rg:q.rg}; repaint(); }
    else if(st.drag.k==="guard"&&q){
      var daz=q.az-st.drag.az, drg=q.rg-st.drag.rg, wid=guard.az1-guard.az0, dep=guard.r1-guard.r0;
      var a0=Math.max(-HALF+0.02,Math.min(HALF-0.02-wid,guard.az0+daz));
      var r0=Math.max(0.10,Math.min(0.96-dep,guard.r0+drg));
      guard.az0=a0; guard.az1=a0+wid; guard.r0=r0; guard.r1=r0+dep;
      st.drag.az=q.az; st.drag.rg=q.rg; repaint();
    }
  });
  function endPointer(){ st.drag=null; if(st.hold){ st.hold=null; } repaint(); }
  cv.addEventListener("pointerup",endPointer);
  cv.addEventListener("pointercancel",endPointer);
  cv.addEventListener("pointerleave",function(){ st.hover=null; st.hoverTrk=-1; if(st.hold){st.hold=null;} repaint(); });
  cv.addEventListener("contextmenu",function(e){
    e.preventDefault();
    var q=inWedge(pos(e));
    if(q){ st.cue={az:q.az,left:2.4}; repaint(); }
  });
  cv.addEventListener("wheel",function(e){
    var p=pos(e), c=hitCtrl(p);
    if(c&&c.k==="range"){
      e.preventDefault();
      st.rangeKm=Math.max(0.2,Math.min(3.0, st.rangeKm + (e.deltaY<0?0.05:-0.05)));
      st.rangeKm=Math.round(st.rangeKm*100)/100;
      st.rangeFine=(RANGES.indexOf(st.rangeKm)<0);
      repaint();
    }
  },{passive:false});
  cv.setAttribute("tabindex","0");
  cv.addEventListener("keydown",function(e){
    var k=e.key.toLowerCase(), used=true;
    if(k>="1"&&k<="4") select(+k-1);
    else if(k==="v") st.video=(st.video+1)%2;
    else if(k==="d") st.decay=(st.decay+1)%3;
    else if(k==="s") st.show=(st.show+1)%3;
    else if(k==="l") st.listen=!st.listen;
    else if(k==="[") stepRange(-1);
    else if(k==="]") stepRange(1);
    else if(k==="a"){ if(!st.hold) startHold("ack"); }
    else if(k==="f"){ if(!st.hold) startHold("fault"); }
    else if(k==="escape"){ st.ebl=null; st.cue=null; }
    else used=false;
    if(used){ e.preventDefault(); st.focusKb=true; repaint(); }
  });
  cv.addEventListener("keyup",function(e){
    var k=e.key.toLowerCase();
    if((k==="a"||k==="f")&&st.hold){ st.hold=null; repaint(); }
  });
  cv.addEventListener("blur",function(){ st.hold=null; st.focusKb=false; repaint(); });

  /* ---------- drawing ---------- */
  function A(az){ return -Math.PI/2+az; }                     /* bearing to canvas angle */
  function px(az,rg){ var a=A(az); return [cx+Math.cos(a)*rg*R, cy+Math.sin(a)*rg*R]; }
  function wedgePath(az0,az1,r0,r1){
    ctx.beginPath();
    ctx.arc(cx,cy,r1*R,A(az0),A(az1));
    ctx.arc(cx,cy,r0*R,A(az1),A(az0),true);
    ctx.closePath();
  }
  /* A label on the picture needs its own ground or the video eats it. */
  function stamp(s,x,y,c2,size,align){
    mono(ctx,size||9.5,"700");
    var w2=ctx.measureText(s).width, ax=align==="center"?x-w2/2:x;
    ctx.fillStyle="rgba(8,11,10,0.72)"; ctx.fillRect(ax-4,y-9,w2+8,13);
    txt(ctx,s,x,y,c2,size||9.5,"700",align);
  }
  function ctrlBox(c,fill,edge){
    ctx.fillStyle=fill; ctx.fillRect(c.x,c.y,c.w,c.h);
    ctx.strokeStyle=edge; ctx.lineWidth=1; ctx.strokeRect(c.x+.5,c.y+.5,c.w-1,c.h-1);
  }

  var lastT=0;
  function repaint(){ frame(lastT); }

  function frame(t){
    var dt=Math.max(0,Math.min(80,t-lastT))/1000; lastT=t;

    /* ---- run the little world forward ---- */
    if(st.hold){
      if(st.hold.t0===null) st.hold.t0=t;
      st.hold.p=Math.min(1,(t-st.hold.t0)/1200);
      if(st.hold.p>=1){ var kk=st.hold.kind; st.hold=null; finishHold(kk); }
    }
    if(st.gainAuto) st.gain=0.55+0.13*Math.sin(t/2600);
    if(st.cue){ st.cue.left-=dt; if(st.cue.left<=0) st.cue=null; }

    trk.forEach(function(g){
      if(g.state==="dropped") return;
      if(g.state==="coast"){ g.coast-=dt; if(g.coast<=0){ g.state="dropped"; } }
      var drg=(g.mps/(st.rangeKm*1000))*dt;
      g.rg+=drg;
      if(g.rg<0.12) g.rg=0.98;
      if(g.rg>1.02) g.rg=0.98;
      g.az+=g.vaz*dt;
      if(g.az> HALF-0.05){ g.az= HALF-0.05; g.vaz*=-1; }
      if(g.az<-HALF+0.05){ g.az=-HALF+0.05; g.vaz*=-1; }
    });

    /* the tripwire decides, not the designer */
    var tripped=trk.some(function(g){
      return g.state==="live"&&g.az>guard.az0&&g.az<guard.az1&&g.rg>guard.r0&&g.rg<guard.r1;
    });
    if(tripped&&st.alarm==="none"){ st.alarm="unack"; }
    if(!tripped&&st.alarm!=="none"){ st.alarm="none"; }

    if(mdCls!==sel().cls) mdWarm(sel().cls);

    var flash=(Math.sin(t/170)>0);          /* the unacknowledged blink */
    var live=trk.filter(function(g){return g.state!=="dropped";});

    ctx.fillStyle=C.tube; ctx.fillRect(0,0,W,H);

    /* ================= top status strip ================= */
    line(ctx,0,28,W,28,C.dim,1);
    txt(ctx,"5.8 GHz  ·  FMCW  ·  2 TX / 2 RX",12,19,C.phos,11,"700");
    txt(ctx,"SECTOR ±45°",290,19,"#6E8078",11,"600");
    txt(ctx,"STARING  ·  NO SCAN",400,19,"#6E8078",11,"600");
    txt(ctx,st.rangeKm.toFixed(st.rangeFine?2:1)+" km",560,19,st.rangeFine?C.white:"#6E8078",11,"600");
    txt(ctx,live.length+" TRACK"+(live.length===1?"":"S"),640,19,"#6E8078",11,"600");
    txt(ctx,"REC",W-40,19,C.red,11,"700");
    ctx.fillStyle=C.red; ctx.beginPath(); ctx.arc(W-52,15,4,0,Math.PI*2); ctx.fill();

    /* ================= banner: the machine's own state ================= */
    var bx=14, bw=SP.w-28, by=38, bh=32;
    var bnr=null;
    if(st.fault) bnr=["REF UNLOCK  ·  BEARINGS UNTRUSTWORTHY  ·  TRACKS COASTING",C.red,true];
    else if(st.alarm==="unack") bnr=["GUARD ZONE  ·  "+
        (trk.filter(function(g){return g.state==="live"&&g.az>guard.az0&&g.az<guard.az1&&g.rg>guard.r0&&g.rg<guard.r1;})
          .map(function(g){return g.id;}).join(", "))+"  ·  HOLD ALARM TO ACKNOWLEDGE",C.red,true];
    else if(st.alarm==="ack") bnr=["GUARD ZONE  ·  ACKNOWLEDGED  ·  STILL PRESENT",C.orange,false];
    if(bnr){
      var lit=bnr[2]?flash:true;
      ctx.fillStyle=bnr[1]===C.red?(lit?"rgba(255,19,32,0.20)":"rgba(255,19,32,0.07)"):"rgba(254,147,13,0.13)";
      ctx.fillRect(bx,by,bw,bh);
      ctx.strokeStyle=bnr[1]; ctx.globalAlpha=lit?1:0.45; ctx.strokeRect(bx+.5,by+.5,bw-1,bh-1); ctx.globalAlpha=1;
      txt(ctx,bnr[0],bx+14,by+21,bnr[1],11.5,"700");
    } else {
      ctx.strokeStyle=C.dim; ctx.strokeRect(bx+.5,by+.5,bw-1,bh-1);
      txt(ctx,"NO ACTIVE ALERTS",bx+14,by+21,"#4E6058",11.5,"700");
      txt(ctx,"BUDGET  6 / HOUR  ·  2 USED",bx+bw-14,by+21,"#3E5049",10.5,"700","right");
    }

    /* ================= cue: a look asked for, not yet had ================= */
    var qy=by+bh+12;
    if(st.cue){
      ctx.fillStyle="rgba(7,205,237,0.10)"; ctx.fillRect(bx,qy,bw,26);
      ctx.strokeStyle=C.aqua; ctx.strokeRect(bx+.5,qy+.5,bw-1,25);
      txt(ctx,"CUE REQUESTED  "+(st.cue.az*180/Math.PI>=0?"+":"")+
        (st.cue.az*180/Math.PI).toFixed(0)+"°  ·  DWELLING  ·  "+st.cue.left.toFixed(1)+" s",
        bx+14,qy+18,C.aqua,11,"700");
    } else {
      txt(ctx,"NO OUTSTANDING CUE  ·  RIGHT-CLICK THE PICTURE TO ASK FOR A LOOK",bx+2,qy+18,"#3E5049",10.5,"700");
    }
    txt(ctx,"DWELL 1.0 s  ·  REVISIT CONTINUOUS  ·  EVERY BEARING WATCHED AT ONCE",
        bx+2,qy+44,"#3E5049",10.5,"700");

    /* ================= the picture ================= */
    /* the sector itself, and a slow breath across all of it — nothing sweeps */
    ctx.save();
    wedgePath(-HALF,HALF,0,1);
    ctx.fillStyle="rgba(53,224,122,"+(0.028+0.010*Math.sin(t/1900)).toFixed(4)+")"; ctx.fill();
    ctx.clip();

    /* --- video --- */
    if(st.show===0){
      var gset=0.45+st.gain*1.25;
      clutter.forEach(function(c2){
        var p2=px(c2.az,c2.rg);
        var a=Math.min(0.72,c2.amp*gset*0.42);
        ctx.fillStyle="rgba(53,224,122,"+a.toFixed(3)+")";
        ctx.fillRect(p2[0]-1.4,p2[1]-1.4,2.8,2.8);
      });
      /* what moved: the live return minus its own running average */
      if(st.video===1){
        var steps = st.decay===2?1 : st.decay===1?4 : 9;
        live.forEach(function(g){
          for(var s2=0;s2<steps;s2++){
            var back=g.rg - (g.mps/(st.rangeKm*1000))*(s2*(st.decay===1?0.34:0.14));
            var p3=px(g.az-g.vaz*(s2*0.14),back);
            var a3=st.decay===2?0.55:(0.55*Math.pow(1-s2/steps,st.decay===1?1:1.7));
            ctx.fillStyle="rgba(255,19,32,"+a3.toFixed(3)+")";
            var sz=st.decay===1?3.4:3.0;
            ctx.fillRect(p3[0]-sz/2,p3[1]-sz/2,sz,sz);
          }
        });
      }
    }

    /* --- the blind region behind the roofline --- */
    if(st.show<2){
      ctx.beginPath();
      shadow.pts.forEach(function(p4,i){ var q4=px(p4[0],p4[1]); i?ctx.lineTo(q4[0],q4[1]):ctx.moveTo(q4[0],q4[1]); });
      var last=px(shadow.a1,1.02), first=px(shadow.a0,1.02);
      ctx.lineTo(last[0],last[1]); ctx.lineTo(first[0],first[1]); ctx.closePath();
      ctx.save(); ctx.clip();
      for(var hx=-R*2;hx<R*2;hx+=7){
        line(ctx,cx+hx,cy-R*1.2,cx+hx+R*1.2,cy+R*0.2,"rgba(223,243,52,0.10)",1);
      }
      ctx.restore();
      ctx.strokeStyle="rgba(223,243,52,0.40)"; ctx.lineWidth=1.2; ctx.setLineDash([5,4]); ctx.stroke(); ctx.setLineDash([]);
    }
    ctx.restore();

    /* --- wedge edges, range arcs, bearing scale --- */
    [-HALF,HALF].forEach(function(h){
      var p5=px(h,1); line(ctx,cx,cy,p5[0],p5[1],"rgba(53,224,122,0.42)",1.2);
    });
    for(var k2=1;k2<=4;k2++){
      ctx.save(); ctx.beginPath();
      ctx.arc(cx,cy,R*k2/4,A(-HALF),A(HALF));
      ctx.strokeStyle="rgba(53,224,122,0.20)"; ctx.lineWidth=1; ctx.stroke(); ctx.restore();
      txt(ctx,(st.rangeKm*k2/4).toFixed(2)+" km",cx+18,cy-R*k2/4+15,"rgba(53,224,122,0.45)",10,"600");
    }
    /* the bearing scale sits outside the picture, and goes yellow the moment the
       machine stops being sure of it */
    var bcol=st.fault?C.yellow:"rgba(53,224,122,0.5)";
    for(var b2=-45;b2<=45;b2+=15){
      var az2=b2*Math.PI/180, p6=px(az2,1), p7=px(az2,1.024);
      line(ctx,p6[0],p6[1],p7[0],p7[1],st.fault?C.yellow:"rgba(53,224,122,0.4)",1);
      var p8=px(az2,1.065);
      txt(ctx,(b2>0?"+":"")+b2,p8[0],p8[1]+4,bcol,10,"600","center");
    }
    if(st.fault) txt(ctx,"BEARING SCALE UNTRUSTED",cx,cy-R-30,C.yellow,10,"700","center");

    /* how far the returns actually reach today, which is not the range you chose */
    ctx.save(); ctx.beginPath();
    var edgeR=Math.min(0.99,(0.94/st.rangeKm));
    ctx.arc(cx,cy,R*edgeR,A(-HALF),A(HALF));
    ctx.setLineDash([9,6]); ctx.strokeStyle="rgba(255,19,32,0.45)"; ctx.lineWidth=1.3; ctx.stroke(); ctx.restore();
    if(edgeR<0.985){
      var pe=px(-HALF*0.72,edgeR);
      stamp("DETECTION EDGE  0.94 km",pe[0],pe[1]-8,"rgba(255,19,32,0.68)",9.5);
    }

    /* --- the tripwire the operator owns --- */
    var showGuard = st.show<2 || st.alarm!=="none";
    if(showGuard){
      var glit = st.alarm==="unack" ? (flash?1:0.35) : st.alarm==="ack" ? 0.9 : 0.55;
      ctx.save(); wedgePath(guard.az0,guard.az1,guard.r0,guard.r1);
      ctx.globalAlpha=glit;
      ctx.fillStyle=st.alarm==="none"?"rgba(255,165,31,0.055)":"rgba(255,19,32,0.13)";
      ctx.fill();
      ctx.strokeStyle=st.alarm==="none"?C.amber:C.red; ctx.lineWidth=1.3; ctx.stroke();
      ctx.globalAlpha=1; ctx.restore();
      var gm=px((guard.az0+guard.az1)/2,guard.r1);
      stamp("GUARD",gm[0],gm[1]-9,st.alarm==="none"?C.amber:C.red,9.5,"center");
      if(st.hover==="picture"||(st.drag&&st.drag.k==="guard")){
        [[guard.az0,guard.r0],[guard.az1,guard.r0],[guard.az0,guard.r1],[guard.az1,guard.r1]].forEach(function(h2){
          var ph=px(h2[0],h2[1]);
          ctx.fillStyle=C.amber; ctx.fillRect(ph[0]-3,ph[1]-3,6,6);
        });
      }
      if(st.show===2&&st.alarm!=="none")
        txt(ctx,"GUARD FORCED BACK ON BY THE ALARM",cx,cy+34,C.red,10,"700","center");
    }
    if(st.show<2){
      var sm=px((shadow.a0+shadow.a1)/2,0.88);
      stamp("ROOFLINE SHADOW · COMPUTED",sm[0],sm[1],"rgba(223,243,52,0.62)",9.5,"center");
    }

    /* --- the two rulers, told apart by their dashes --- */
    if(st.ebl){
      var pb=px(st.ebl.az,1);
      ctx.save(); ctx.setLineDash([10,5]); ctx.strokeStyle=C.aqua; ctx.lineWidth=1.1;
      ctx.beginPath(); ctx.moveTo(cx,cy); ctx.lineTo(pb[0],pb[1]); ctx.stroke();
      ctx.setLineDash([2,4]);
      ctx.beginPath(); ctx.arc(cx,cy,st.ebl.rg*R,A(-HALF),A(HALF)); ctx.stroke();
      ctx.restore();
      var pk=px(st.ebl.az,st.ebl.rg);
      ctx.strokeStyle=C.aqua; ctx.lineWidth=1;
      line(ctx,pk[0]-7,pk[1],pk[0]+7,pk[1],C.aqua,1);
      line(ctx,pk[0],pk[1]-7,pk[0],pk[1]+7,C.aqua,1);
    }

    /* --- tracks --- */
    trk.forEach(function(g,i){
      if(g.state==="dropped"){ g._x=undefined; return; }
      var p9=px(g.az,g.rg), x=p9[0], y=p9[1], c3=col[g.cls], isSel=(i===st.sel);
      g._x=x; g._y=y;
      var coasting=(g.state==="coast");
      var inZone=g.state==="live"&&g.az>guard.az0&&g.az<guard.az1&&g.rg>guard.r0&&g.rg<guard.r1;
      var blink = inZone&&st.alarm==="unack" ? flash : true;
      ctx.globalAlpha=blink?1:0.28;

      /* where it has been: equally spaced in time, so uneven spacing is acceleration */
      for(var h3=1;h3<=4;h3++){
        var hr=g.rg-(g.mps/(st.rangeKm*1000))*h3*0.9;
        var ph2=px(g.az-g.vaz*h3*0.9,hr);
        ctx.globalAlpha=(blink?1:0.28)*(0.34-h3*0.062);
        ctx.fillStyle=c3; ctx.fillRect(ph2[0]-1.6,ph2[1]-1.6,3.2,3.2);
      }
      ctx.globalAlpha=blink?1:0.28;

      /* where it will be, with the horizon stated on the picture */
      if(!coasting){
        var vr=g.rg+(g.mps/(st.rangeKm*1000))*8;
        var pv=px(g.az+g.vaz*8,Math.max(0.04,vr));
        line(ctx,x,y,pv[0],pv[1],c3,1.2);
        ctx.fillStyle=c3; ctx.beginPath();
        ctx.arc(pv[0],pv[1],2,0,Math.PI*2); ctx.fill();
      }

      ctx.strokeStyle=c3; ctx.lineWidth=isSel?2.3:1.6;
      ctx.save();
      if(g.cls==="suspect"||coasting) ctx.setLineDash([3,2.6]);
      ctx.beginPath();
      if(g.cls==="drone"||g.cls==="suspect"){
        ctx.moveTo(x,y-10); ctx.lineTo(x+9,y+2); ctx.lineTo(x,y+9); ctx.lineTo(x-9,y+2); ctx.closePath();
      } else if(g.cls==="bird"){ ctx.arc(x,y,8.5,0,Math.PI*2); }
      else { ctx.rect(x-8,y-8,16,16); }
      ctx.stroke(); ctx.restore();
      if(isSel) ring(ctx,x,y,17,c3,1.2,[3,3]);
      if(st.hoverTrk===i&&!isSel) ring(ctx,x,y,17,"rgba(255,255,255,0.35)",1);

      txt(ctx,g.id,x+15,y-8,c3,11,"700");
      if(coasting) txt(ctx,"COAST "+Math.ceil(g.coast)+" s",x+15,y+6,C.yellow,10,"700");
      else txt(ctx,Math.abs(g.mps).toFixed(1)+" m/s   "+g.alt+" m",x+15,y+6,c3,10,"600");
      ctx.globalAlpha=1;
    });

    /* the aerial itself */
    ctx.fillStyle=st.fault?C.yellow:C.phos; ctx.fillRect(cx-9,cy-3,18,7);
    txt(ctx,"ARRAY",cx,cy+21,"#6E8078",9.5,"700","center");

    /* ================= range vs speed ================= */
    panel(ctx,RD.x,RD.y+14,RD.w,RD.h-14);
    txt(ctx,"RANGE × SPEED",RD.x+10,RD.y+8,"#6E8078",10,"700");
    var rdx=RD.x+1, rdy=RD.y+15, rdw=RD.w-2, rdh=RD.h-16;
    ctx.fillStyle="#050807"; ctx.fillRect(rdx,rdy,rdw,rdh);
    for(var gy=0;gy<rdh;gy+=2){ ctx.fillStyle="rgba(53,224,122,0.13)"; ctx.fillRect(rdx+rdw/2-3,rdy+gy,6,1.5); }
    line(ctx,rdx+rdw/2,rdy,rdx+rdw/2,rdy+rdh,"rgba(53,224,122,0.25)",1);
    for(var yk=1;yk<=3;yk++){
      var yy2=rdy+rdh-rdh*yk/4;
      line(ctx,rdx,yy2,rdx+6,yy2,"rgba(53,224,122,0.28)",1);
      txt(ctx,(st.rangeKm*yk/4).toFixed(2),rdx+10,yy2+4,"rgba(53,224,122,0.35)",9,"600");
    }
    trk.forEach(function(g,i){
      if(g.state==="dropped") return;
      var v=Math.max(-1,Math.min(1,g.mps/-18));
      var x2=rdx+rdw/2+v*rdw*0.42, y2=rdy+rdh-g.rg*rdh, c4=col[g.cls];
      var grd=ctx.createRadialGradient(x2,y2,0,x2,y2,14);
      grd.addColorStop(0,"rgba(255,255,255,0.95)");
      grd.addColorStop(0.35,c4); grd.addColorStop(1,"rgba(0,0,0,0)");
      ctx.globalAlpha=g.state==="coast"?0.4:0.85; ctx.fillStyle=grd;
      ctx.beginPath(); ctx.arc(x2,y2,14,0,Math.PI*2); ctx.fill(); ctx.globalAlpha=1;
      if(i===st.sel) ring(ctx,x2,y2,17,c4,1.2,[3,3]);
    });
    txt(ctx,"GROUND",rdx+rdw/2+6,rdy+14,"rgba(53,224,122,0.45)",9,"600");
    txt(ctx,"← AWAY",rdx+6,rdy+rdh-8,"#4E6058",9,"600");
    txt(ctx,"TOWARDS →",rdx+rdw-6,rdy+rdh-8,"#4E6058",9,"600","right");

    /* ================= the speed signature of the selected track ================= */
    var g0=sel();
    panel(ctx,MD.x,MD.y+14,MD.w,MD.h-14);
    txt(ctx,"SPEED SIGNATURE  ·  "+g0.id,MD.x+10,MD.y+8,"#6E8078",10,"700");
    if(!reduce) mdColumn(t,g0.cls);
    ctx.drawImage(mdBuf,MD.x+1,MD.y+15);
    line(ctx,MD.x+1,MD.y+15+mdMid,MD.x+MD.w-1,MD.y+15+mdMid,"rgba(255,255,255,0.16)",1,[4,5]);
    txt(ctx,"+ TOWARDS",MD.x+8,MD.y+28,"#3E5049",9,"700");
    txt(ctx,"− AWAY",MD.x+8,MD.y+13+mdBuf.height-6,"#3E5049",9,"700");
    var cap=MDCAP[g0.cls];
    txt(ctx,cap[0],MD.x+10,MD.y+MD.h-24,cap[1],10,"700");
    txt(ctx,"← 3 s",MD.x+MD.w-10,MD.y+MD.h-24,"#4E6058",9.5,"600","right");
    if(st.listen){
      txt(ctx,"♪  DOPPLER TONE ROUTED TO HEADSET",MD.x+10,MD.y+MD.h-8,C.aqua,10,"700");
    } else {
      txt(ctx,"AUDIO OFF",MD.x+10,MD.y+MD.h-8,"#3E5049",10,"700");
    }

    /* ================= measure: what the rulers say ================= */
    panel(ctx,AUX.x,AUX.y+14,AUX.w,AUX.h-14);
    txt(ctx,"MEASURE",AUX.x+10,AUX.y+8,"#6E8078",10,"700");
    if(st.ebl){
      var degs=st.ebl.az*180/Math.PI, kms=st.ebl.rg*st.rangeKm;
      txt(ctx,"BEARING",AUX.x+12,AUX.y+40,"#5E7269",9,"700");
      txt(ctx,(degs>=0?"+":"")+degs.toFixed(1)+"°",AUX.x+12,AUX.y+60,C.aqua,15,"700");
      txt(ctx,"RANGE",AUX.x+110,AUX.y+40,"#5E7269",9,"700");
      txt(ctx,kms.toFixed(3)+" km",AUX.x+110,AUX.y+60,C.aqua,15,"700");
      txt(ctx,"BY HAND  ·  ESC CLEARS",AUX.x+12,AUX.y+80,"#3E5049",9.5,"700");
    } else {
      txt(ctx,"DRAG THE PICTURE TO MEASURE",AUX.x+12,AUX.y+46,"#4E6058",11,"700");
      txt(ctx,"A BEARING LINE AND A RANGE RING,",AUX.x+12,AUX.y+64,"#3E5049",9.5,"700");
      txt(ctx,"BECAUSE THE NUMBER THAT MATTERS",AUX.x+12,AUX.y+78,"#3E5049",9.5,"700");
      txt(ctx,"IS RARELY ONE THAT WAS PRECOMPUTED.",AUX.x+12,AUX.y+92,"#3E5049",9.5,"700");
    }

    /* ================= track list ================= */
    line(ctx,TL.x-10,28,TL.x-10,CTRL_Y,C.dim,1);
    txt(ctx,"TRACKS",TL.x,TL.y+22,"#5E7269",10,"700");
    txt(ctx,"1-4",TL.x+TL.w-2,TL.y+22,"#3E5049",9.5,"700","right");
    trk.forEach(function(g,i){
      var y3=TL.y+44+i*76, c5=col[g.cls], dead=(g.state==="dropped");
      if(i===st.sel&&!dead){
        ctx.fillStyle="rgba(255,255,255,0.06)"; ctx.fillRect(TL.x-10,y3-18,TL.w+20,68);
        ctx.fillStyle=c5; ctx.fillRect(TL.x-10,y3-18,2.5,68);
      }
      if(st.hoverTrk===i&&i!==st.sel&&!dead){
        ctx.fillStyle="rgba(255,255,255,0.025)"; ctx.fillRect(TL.x-10,y3-18,TL.w+20,68);
      }
      ctx.globalAlpha=dead?0.42:1;
      txt(ctx,g.id,TL.x,y3,dead?C.grey:c5,13,"700");
      if(dead){
        line(ctx,TL.x,y3-4,TL.x+52,y3-4,C.grey,1);
        txt(ctx,"DROPPED",TL.x,y3+16,C.grey,10,"700");
        txt(ctx,"NO UPDATE 8 s",TL.x,y3+32,"#5E7269",9.5,"600");
      } else if(g.state==="coast"){
        txt(ctx,"COASTING",TL.x,y3+16,C.yellow,10,"700");
        txt(ctx,"DIES IN "+Math.ceil(g.coast)+" s",TL.x,y3+32,C.yellow,9.5,"600");
        txt(ctx,"*.* m/s",TL.x+88,y3+16,C.yellow,10.5,"700");
        txt(ctx,g.alt+" m agl",TL.x+88,y3+32,"#5E7269",10.5,"600");
      } else {
        txt(ctx,CLSNAME[g.cls],TL.x,y3+16,c5,10,"700");
        txt(ctx,g.cf===null?"conf —":"conf "+g.cf.toFixed(2),TL.x,y3+32,"#6E8078",10,"600");
        txt(ctx,Math.abs(g.mps).toFixed(1)+" m/s",TL.x+88,y3+16,"#8FA39B",10.5,"600");
        txt(ctx,g.alt+" m agl",TL.x+88,y3+32,"#8FA39B",10.5,"600");
      }
      ctx.globalAlpha=1;
      line(ctx,TL.x-10,y3+50,TL.x+TL.w,y3+50,C.dim,1);
    });
    txt(ctx,"BUILT FROM",TL.x,TL.y+TL.h-52,"#5E7269",9,"700");
    ["RNG","DOP","uD","CAM"].forEach(function(s3,i){
      var on=(i<3)&&!st.fault&&sel().state==="live"&&!(i===2&&sel().cls==="unk");
      txt(ctx,on?"●":"○",TL.x+i*40,TL.y+TL.h-32,on?C.phos:"#3E5049",11,"700");
      txt(ctx,s3,TL.x+i*40,TL.y+TL.h-18,on?"#8FA39B":"#3E5049",8.5,"700");
    });

    /* ================= the control bar ================= */
    line(ctx,0,CTRL_Y,W,CTRL_Y,C.dim,1);
    CTRLS.forEach(function(c6){
      var hot=(st.hover===c6.k);
      var val=ctrlValue(c6), disabled=(c6.k==="ack"&&st.alarm==="none");
      var accent="#8FA39B", fill="#0E1413", edge=hot?"#3B4E47":"#22302C";
      if(c6.k==="ack"&&st.alarm==="unack"){ accent=C.red; edge=C.red; fill=flash?"rgba(255,19,32,0.17)":"rgba(255,19,32,0.06)"; }
      else if(c6.k==="ack"&&st.alarm==="ack"){ accent=C.orange; edge="#5A4A14"; }
      else if(c6.k==="fault"&&st.fault){ accent=C.red; edge=C.red; fill="rgba(255,19,32,0.10)"; }
      else if(c6.k==="listen"&&st.listen){ accent=C.aqua; edge="#0B4E5A"; fill="rgba(7,205,237,0.09)"; }
      else if(c6.k==="gain"&&st.gainAuto){ accent=C.yellow; edge="#5A4A14"; }
      if(disabled) accent="#3E5049";
      txt(ctx,c6.name,c6.x,CTRL_Y+16,hot?"#8FA39B":"#5E7269",9,"700");
      ctrlBox(c6,fill,edge);

      if(c6.k==="gain"){
        var bw2=c6.w-52;
        ctx.fillStyle="#16211D"; ctx.fillRect(c6.x+8,c6.y+21,bw2,5);
        ctx.fillStyle=st.gainAuto?C.yellow:C.phos; ctx.fillRect(c6.x+8,c6.y+21,bw2*st.gain,5);
        ctx.fillStyle=st.gainAuto?C.yellow:"#DCE5E0";
        ctx.fillRect(c6.x+8+bw2*st.gain-1.5,c6.y+17,3,13);
        txt(ctx,val,c6.x+8,c6.y+15,accent,11.5,"700");
        var tg=st.gainAuto?"AUTO":"MAN";
        ctx.fillStyle=st.gainAuto?"rgba(223,243,52,0.16)":"#16211D";
        ctx.fillRect(c6.x+c6.w-42,c6.y+6,36,22);
        txt(ctx,tg,c6.x+c6.w-24,c6.y+20,st.gainAuto?C.yellow:"#8FA39B",10,"700","center");
      } else if(c6.k==="range"){
        txt(ctx,"‹",c6.x+9,c6.y+23,"#6E8078",15,"700");
        txt(ctx,"›",c6.x+c6.w-9,c6.y+23,"#6E8078",15,"700","right");
        txt(ctx,val,c6.x+c6.w/2,c6.y+22,st.rangeFine?C.white:accent,12,"700","center");
        if(st.rangeFine) txt(ctx,"FINE",c6.x+c6.w/2,c6.y+32,C.white,7.5,"700","center");
      } else {
        txt(ctx,val,c6.x+c6.w/2,c6.y+22,accent,11,"700","center");
      }
      /* a guarded action fills its own control rather than opening a dialog */
      if(st.hold&&st.hold.kind===c6.k){
        ctx.fillStyle=c6.k==="fault"?"rgba(255,19,32,0.30)":"rgba(255,19,32,0.34)";
        ctx.fillRect(c6.x+1,c6.y+c6.h-4,(c6.w-2)*st.hold.p,3);
      }
    });

    /* ================= health: the state of the machine, always on the glass ================= */
    line(ctx,0,HEALTH_Y,W,HEALTH_Y,C.dim,1);
    var lights=[["SIGNAL",true],["SWEEP",true],["KEEPING UP",true],
                [st.fault?"REF UNLOCK":"CALIBRATED",!st.fault?null:false]];
    lights.forEach(function(l,i){
      var x3=14+i*116, ok=l[1], bad=(ok===false), warn=(ok===null);
      var fg=bad?C.red:warn?C.yellow:C.green;
      ctx.fillStyle=bad?"#2A1113":warn?"#2A2410":"#12251C"; ctx.fillRect(x3,HEALTH_Y+16,104,26);
      ctx.strokeStyle=bad?"#5A1A1E":warn?"#5A4A14":"#2C5C42"; ctx.strokeRect(x3+.5,HEALTH_Y+16.5,103,25);
      var lit=bad?flash:true;
      ctx.globalAlpha=lit?1:0.35;
      ctx.fillStyle=fg; ctx.beginPath(); ctx.arc(x3+13,HEALTH_Y+29,5,0,Math.PI*2); ctx.fill();
      ctx.globalAlpha=1;
      txt(ctx,l[0],x3+24,HEALTH_Y+33,fg,10.5,"700");
    });
    var vals=[["RX GAIN",(st.gain*100).toFixed(0)+" %",st.gainAuto?C.yellow:C.phos],
              ["NOISE","-96 dBm",C.phos],
              ["TEMP","41 °C",C.phos],
              [st.fault?"CAL LOST":"CAL AGE",st.fault?"*.*":"4 h 12 m",st.fault?C.red:C.yellow],
              ["CPU","46 %",C.phos],
              ["DROPPED",String(st.dropped),st.dropped?C.yellow:C.phos]];
    vals.forEach(function(it,i){
      var x4=494+i*106;
      txt(ctx,it[0],x4,HEALTH_Y+24,"#5E7269",9,"700");
      txt(ctx,it[1],x4,HEALTH_Y+42,it[2],13,"700");
    });
    txt(ctx, st.fault ? "TIMING REFERENCE LOST — EVERY BEARING BELOW IS DEAD RECKONED"
                      : "CALIBRATION AGEING — BEARINGS MAY DRIFT",
        14,HEALTH_Y+62,st.fault?C.red:C.yellow,10,"700");

    /* ================= guidance: what the buttons do, right now ================= */
    line(ctx,0,GUIDE_Y,W,GUIDE_Y,C.dim,1);
    ctx.fillStyle="#0B100E"; ctx.fillRect(0,GUIDE_Y+1,W,H-GUIDE_Y-1);
    var gtext, gcol="#8FA39B";
    if(st.drag&&st.drag.k==="ebl"){ gtext="MEASURING  ·  RELEASE TO KEEP THE READING  ·  ESC CLEARS IT"; gcol=C.aqua; }
    else if(st.drag&&st.drag.k==="guard"){ gtext="MOVING THE GUARD ZONE  ·  RELEASE TO SET IT"; gcol=C.amber; }
    else if(st.drag&&st.drag.k==="gain"){ gtext="SETTING GAIN BY HAND  ·  THE TAG SAYS MAN, SO YOU OWN THIS NUMBER"; gcol=C.phos; }
    else if(st.hold){ gtext="HOLD  ·  RELEASE TO ABANDON"; gcol=C.red; }
    else if(st.hover&&CTRLHELP[st.hover]){ gtext=CTRLHELP[st.hover].toUpperCase(); }
    else if(st.hover==="picture"&&st.hoverTrk>=0){ gtext="LEFT  select "+trk[st.hoverTrk].id+"        RIGHT  cue the array to this bearing"; }
    else if(st.hover==="picture"){ gtext="LEFT  drag to measure, or drag the guard zone        RIGHT  cue the array to this bearing"; }
    else gtext="LEFT  select a track  ·  drag to measure        RIGHT  cue a bearing        KEYS  1-4 · V · D · S · L · A · F · [ ]";
    txt(ctx,gtext,14,GUIDE_Y+26,gcol,10.5,"700");
    txt(ctx,reduce?"STILL FRAME  ·  CONTROLS STILL LIVE":"LIVE",W-14,GUIDE_Y+26,"#3E5049",10,"700","right");
  }

  mdWarm(trk[0].cls);
  run(cv, frame);
})();

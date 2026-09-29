// s10_tuner.js — ของเล่นจูน 3 ค่าในสไลด์ session-10 (ภาคผนวก)
// สูตรเดียวกับ solution_codes/shooter_step2.py:28-32 · ก้าวละ ~33 ms ให้ใกล้ 30 fps ของ game.run()
(function(){
var box=document.getElementById('tuner'); if(!box||box.dataset.i)return; box.dataset.i=1;
var As=document.getElementById('tAs'),Fs=document.getElementById('tFs'),Ms=document.getElementById('tMs');
var Av=document.getElementById('tA'),Fv=document.getElementById('tF'),Mv=document.getElementById('tM');
var c=document.getElementById('tunecv'),x=4,speed=0,t0=null,last=null,cycle=0;
function use(){return {A:+As.value,F:+Fs.value,M:+Ms.value};}
function lbl(){var p=use();Av.textContent=p.A.toFixed(1);Fv.textContent=p.F.toFixed(2);Mv.textContent=p.M.toFixed(0);}
[As,Fs,Ms].forEach(function(s){s.addEventListener('input',lbl);});
function frame(ts){
  if(t0===null){t0=ts;last=ts;}
  var p=use(),el=(ts-t0)/1000,phase=el%3.2,held=phase<1.4;
  if(Math.floor(el/3.2)!==cycle){cycle=Math.floor(el/3.2);x=4;speed=0;}  // every 3.2 s cycle starts from the left, whatever FRICTION is
  var dpr=window.devicePixelRatio||1,W=c.clientWidth||640,H=110;c.width=W*dpr;c.height=H*dpr;
  var g=c.getContext('2d');g.setTransform(dpr,0,0,dpr,0,0);g.fillStyle='#05070a';g.fillRect(0,0,W,H);
  var SW=52;
  if(ts-last>=33){last=ts;                                   // step at ~30 fps like game.run
    if(held){speed+=p.A;}else{speed*=p.F;}
    speed=Math.max(-p.M,Math.min(p.M,speed));                // ③ speed cap
    x=Math.max(0,Math.min(W-SW,x+speed));                    // ④ move + clamp (speed kept, as in step 2)
  }
  g.strokeStyle='#33404f';g.setLineDash([3,3]);g.beginPath();g.moveTo(1,8);g.lineTo(1,H-8);g.moveTo(W-1,8);g.lineTo(W-1,H-8);g.stroke();g.setLineDash([]);
  g.fillStyle='#161b22';g.fillRect(8,H-16,W-16,8);g.fillStyle=held?'#ff8f00':'#19c8ff';g.fillRect(8,H-16,(W-16)*Math.min(1,Math.abs(speed)/p.M),8);
  g.fillStyle='#50FA7B';g.fillRect(x,40,SW,20);
  requestAnimationFrame(frame);
}
function go(){lbl();requestAnimationFrame(frame);}
if(document.readyState==='complete')go();else window.addEventListener('load',go);
})();

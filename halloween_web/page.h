// Page web servie par le Heltec (HTML + CSS + JS dans un seul fichier)
static const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Halloween LED</title>
<style>
:root{--bg:#120a1c;--card:#1d1230;--line:#33224f;--fg:#f3e9ff;--mut:#a395bd;--or:#ff7a1a;--pu:#a64dff}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:16px/1.4 system-ui,sans-serif}
main{max-width:560px;margin:0 auto;padding:16px}
h1{font-size:1.4rem;margin:0 0 12px}
h2{font-size:.8rem;text-transform:uppercase;letter-spacing:.08em;color:var(--mut);margin:20px 0 8px}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:12px;margin-bottom:8px}
.card.active{border-color:var(--or);box-shadow:0 0 0 1px var(--or)}
.row{display:flex;align-items:center;gap:10px}
.row+.row{margin-top:8px}
.nm{flex:1;font-weight:600}
.dv{width:3.2em;text-align:right;color:var(--mut);font-variant-numeric:tabular-nums}
input[type=range]{flex:1;accent-color:var(--or);min-width:0}
canvas{width:100%;height:28px;border-radius:6px;background:#000;display:block}
.bar{height:6px;border-radius:3px;background:var(--line);margin-top:10px;overflow:hidden}
.bar i{display:block;height:100%;width:0;background:var(--or)}
button{font:inherit;color:var(--fg);background:transparent;border:1px solid var(--line);border-radius:8px;padding:6px 10px;cursor:pointer}
button.on{background:var(--pu);border-color:var(--pu)}
#resume{width:100%;margin-top:8px;background:var(--or);border-color:var(--or);color:#000;font-weight:700}
.sw{position:relative;width:46px;height:26px;flex:none}
.sw input{opacity:0;position:absolute;inset:0;margin:0;cursor:pointer;z-index:1}
.sw span{position:absolute;inset:0;background:var(--line);border-radius:13px;transition:.2s}
.sw span:after{content:"";position:absolute;top:3px;left:3px;width:20px;height:20px;border-radius:50%;background:#fff;transition:.2s}
.sw input:checked+span{background:var(--or)}
.sw input:checked+span:after{transform:translateX(20px)}
.cols{display:flex;gap:10px}
.cols label{flex:1;text-align:center;font-size:.85rem;color:var(--mut)}
.cols input{width:100%;height:44px;border:0;background:none;padding:0}
#off{background:#7a1020;border-radius:8px;padding:8px;text-align:center;margin-bottom:8px}
.sub{color:var(--mut);font-size:.9rem}
</style></head><body><main>
<h1>🎃 Halloween LED</h1>
<div id="off" hidden>Connexion perdue…</div>
<div class="card">
  <div class="row"><div class="nm" id="now">…</div><div class="dv" id="tm" style="width:auto"></div></div>
  <div class="bar"><i id="prog"></i></div>
  <div style="height:10px"></div>
  <canvas id="pv" width="400" height="28"></canvas>
  <button id="resume" hidden>▶ Reprendre le spectacle</button>
</div>
<h2>Luminosité</h2>
<div class="card"><div class="row"><input type="range" id="br" min="10" max="255"><span class="dv" id="bv"></span></div></div>
<h2>Animations</h2>
<div id="list"></div>
<div class="sub" id="total"></div>
<h2>Couleurs</h2>
<div class="card"><div class="cols">
  <label>Orange<input type="color" id="col0"></label>
  <label>Violet<input type="color" id="col1"></label>
  <label>Vert<input type="color" id="col2"></label>
</div></div>
</main>
<script>
const SC=[["🔥","Le réveil des flammes"],["🩸","La vague maudite"],["☠️","Le poison"],["👻","L'apparition"],
["⚡","L'orage hanté"],["🧟","L'invasion zombie"],["🎃","Le chaos"],["💀","Le final maudit"]];
const $=id=>document.getElementById(id);
let LOCK=-1,hold=0,bt=0;
document.addEventListener('input',()=>{hold=Date.now()+1500});
async function call(q){
  try{const r=await fetch(q);apply(await r.json());$('off').hidden=true}
  catch(e){$('off').hidden=false}
}
SC.forEach((s,i)=>{
  const d=document.createElement('div');d.className='card';d.id='c'+i;
  d.innerHTML=`<div class="row"><label class="sw"><input type="checkbox" id="en${i}"><span></span></label>
  <div class="nm">${s[0]} ${s[1]}</div><button id="lp${i}">↻ Boucle</button></div>
  <div class="row"><input type="range" id="du${i}" min="5" max="60"><span class="dv" id="dv${i}"></span></div>`;
  $('list').appendChild(d);
  $('en'+i).onchange=e=>call(`/set?en=${i}&v=${e.target.checked?1:0}`);
  $('du'+i).oninput=e=>{$('dv'+i).textContent=e.target.value+' s'};
  $('du'+i).onchange=e=>call(`/set?du=${i}&s=${e.target.value}`);
  $('lp'+i).onclick=()=>call(`/set?lock=${LOCK===i?-1:i}`);
});
$('resume').onclick=()=>call('/set?lock=-1');
$('br').oninput=e=>{
  $('bv').textContent=Math.round(e.target.value/2.55)+'%';
  const n=Date.now();if(n-bt>150){bt=n;fetch('/set?b='+e.target.value)}
};
$('br').onchange=e=>call('/set?b='+e.target.value);
for(let i=0;i<3;i++)$('col'+i).onchange=e=>call(`/set?col=${i}&c=${e.target.value.slice(1)}`);
function draw(px,n,b){
  const g=$('pv').getContext('2d'),k=b/255,w=400/n;
  for(let i=0;i<n;i++){
    const p=i*6,r=parseInt(px.substr(p,2),16)*k,gg=parseInt(px.substr(p+2,2),16)*k,bb=parseInt(px.substr(p+4,2),16)*k;
    g.fillStyle=`rgb(${r|0},${gg|0},${bb|0})`;g.fillRect(i*w,0,w+1,28);
  }
}
function apply(st){
  LOCK=st.l;
  $('now').textContent=SC[st.s][0]+' '+SC[st.s][1]+(st.l>=0?' (en boucle)':'');
  $('tm').textContent=Math.floor(st.t/1000)+' / '+Math.round(st.d/1000)+' s';
  $('prog').style.width=Math.min(100,100*st.t/st.d)+'%';
  $('resume').hidden=st.l<0;
  draw(st.px,st.n,st.b);
  let tot=0;
  for(let i=0;i<8;i++){
    $('c'+i).classList.toggle('active',i===st.s);
    $('lp'+i).classList.toggle('on',i===st.l);
    if(st.en[i])tot+=st.du[i];
  }
  $('total').textContent='Cycle complet : '+Math.floor(tot/60)+' min '+tot%60+' s';
  if(Date.now()<hold)return;
  $('br').value=st.b;$('bv').textContent=Math.round(st.b/2.55)+'%';
  for(let i=0;i<8;i++){
    $('en'+i).checked=!!st.en[i];$('du'+i).value=st.du[i];$('dv'+i).textContent=st.du[i]+' s';
  }
  for(let i=0;i<3;i++)$('col'+i).value='#'+st.c[i];
}
call('/state');setInterval(()=>call('/state'),500);
</script></body></html>)HTML";

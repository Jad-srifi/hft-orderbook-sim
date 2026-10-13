(() => {
  const $ = selector => document.querySelector(selector);
  let runId = null;
  let snapshots = [];
  let heatmap = null;
  let selected = 0;
  let playTimer = null;
  let depthHits = [];
  let heatmapHits = null;

  const fmt = (value, digits = 2) => window.LobDashboard.formatNumber(value, digits);
  const prettyTime = value => {
    if (value === null || value === undefined || value === '') return 'Timestamp unavailable';
    return String(value);
  };
  const escapeHtml = value => window.LobDashboard.escapeHtml(value);

  function stopPlayback() {
    if (playTimer !== null) clearInterval(playTimer);
    playTimer = null;
  }
  function setButtons(enabled) {
    ['play-btn','pause-btn','reset-btn','prev-btn','next-btn','timeline-slider'].forEach(id => { const node = document.getElementById(id); if (node) node.disabled = !enabled; });
  }
  function showReplayError(message) {
    const error = $('#replay-error');
    if (!error) return;
    error.textContent = message || '';
    error.hidden = !message;
  }
  function canvasContext(canvas) {
    if (!canvas) return null;
    const rect = canvas.getBoundingClientRect();
    const dpr = Math.max(1, window.devicePixelRatio || 1);
    const width = Math.max(1, Math.floor(rect.width));
    const height = Math.max(1, Math.floor(rect.height));
    if (canvas.width !== Math.floor(width * dpr) || canvas.height !== Math.floor(height * dpr)) {
      canvas.width = Math.floor(width * dpr); canvas.height = Math.floor(height * dpr);
    }
    const ctx = canvas.getContext('2d');
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, width, height);
    return {ctx, width, height};
  }
  function drawText(ctx, text, x, y, color = '#8995a7', align = 'left', size = 9) {
    ctx.fillStyle = color; ctx.font = `${size}px Inter, system-ui, sans-serif`; ctx.textAlign = align; ctx.fillText(String(text), x, y);
  }

  function drawDepth(record) {
    const canvas = $('#depth-canvas'); const empty = $('#depth-unavailable');
    if (!canvas || !record) return;
    const hasLevels = record.levels_available && ((record.bids || []).length || (record.asks || []).length);
    if (!record.levels_available) {
      canvas.hidden = true; if (empty) { empty.hidden = false; empty.textContent = 'Per-level book data is missing. This run exposes aggregate depth only, so no price-level chart can be derived.'; }
      depthHits = []; return;
    }
    canvas.hidden = false; if (empty) empty.hidden = true;
    const sized = canvasContext(canvas); if (!sized) return;
    const ctx = sized.ctx;
    const width = sized.width, height = sized.height;
    const left = 48, right = 17, top = 12, bottom = 30;
    const plotW = Math.max(1,width-left-right), plotH = Math.max(1,height-top-bottom);
    const bids = record.bids || [], asks = record.asks || [];
    if (!hasLevels) {
      drawText(ctx, 'No displayed levels at this snapshot', width/2, height/2, '#8b96a8', 'center', 10);
      depthHits = []; return;
    }
    const all = [...bids,...asks];
    const minP = Math.min(...all.map(x=>x.price)), maxP = Math.max(...all.map(x=>x.price));
    const safeMin = minP === maxP ? minP - 0.5 : minP;
    const safeMax = minP === maxP ? maxP + 0.5 : maxP;
    const maxQ = Math.max(1,...all.map(x=>x.quantity));
    const xFor = price => left + ((price-safeMin)/(safeMax-safeMin))*plotW;
    const yFor = qty => top+plotH-(qty/maxQ)*plotH;
    ctx.strokeStyle = '#e9edf3'; ctx.lineWidth = 1;
    for (let i=0;i<=4;i++) {
      const y=top+plotH*i/4; ctx.beginPath();ctx.moveTo(left,y);ctx.lineTo(left+plotW,y);ctx.stroke();
      drawText(ctx, Math.round(maxQ*(1-i/4)).toLocaleString(), left-8, y+3, '#8d98a9', 'right', 8);
    }
    const priceTicks=5;
    for(let i=0;i<=priceTicks;i++){
      const price=safeMin+(safeMax-safeMin)*i/priceTicks, x=left+plotW*i/priceTicks;
      ctx.beginPath();ctx.moveTo(x,top);ctx.lineTo(x,top+plotH);ctx.stroke();
      drawText(ctx,fmt(price),x,height-10,'#8490a3','center',8);
    }
    drawText(ctx,'Displayed quantity',13,top+plotH/2,'#7e899b','center',8);
    const count=Math.max(1,all.length); const barW=Math.max(2,Math.min(13,plotW/(count*1.25)));
    depthHits=[];
    for(const [side,levels,color] of [['bid',bids,'#26bdb7'],['ask',asks,'#e77870']]){
      for(const level of levels){
        const x=xFor(level.price)-barW/2, y=yFor(level.quantity), h=top+plotH-y;
        ctx.fillStyle=color; ctx.globalAlpha=.88; ctx.fillRect(x,y,barW,Math.max(1,h)); ctx.globalAlpha=1;
        depthHits.push({x:x-3,y:Math.max(top,y-4),w:barW+6,h:Math.max(8,h+8),side,price:level.price,quantity:level.quantity,cumulative:levels.filter(item=>side==='bid'?item.price>=level.price:item.price<=level.price).reduce((sum,item)=>sum+item.quantity,0)});
      }
    }
    const spread=record.spread;
    if(record.best_bid!=null && record.best_ask!=null && Number.isFinite(Number(spread))){
      ctx.setLineDash([4,4]);ctx.strokeStyle='#6b78df';ctx.beginPath();ctx.moveTo(xFor(record.best_bid),top);ctx.lineTo(xFor(record.best_bid),top+plotH);ctx.moveTo(xFor(record.best_ask),top);ctx.lineTo(xFor(record.best_ask),top+plotH);ctx.stroke();ctx.setLineDash([]);
    }
  }

  function drawLadder(record) {
    const body=$('#ladder-body'); const count=$('#ladder-level-count');
    if(!body||!record)return;
    if(!record.levels_available){body.innerHTML='<tr><td colspan="4" class="empty-cell">Individual price levels are unavailable for this dataset.</td></tr>';if(count)count.textContent='UNAVAILABLE';return;}
    const bids=(record.bids||[]).slice().sort((a,b)=>b.price-a.price);
    const asks=(record.asks||[]).slice().sort((a,b)=>a.price-b.price);
    const rows=Math.max(bids.length,asks.length,1);
    if(count)count.textContent=`${bids.length+bids.length*0+asks.length} LEVELS`;
    const html=[];
    for(let i=0;i<Math.min(rows,25);i++){
      const bid=bids[i],ask=asks[i];
      html.push(`<tr><td class="bid-quantity">${bid?Number(bid.quantity).toLocaleString():''}</td><td class="bid-price">${bid?fmt(bid.price):''}</td><td class="ask-price">${ask?fmt(ask.price):''}</td><td class="ask-quantity">${ask?Number(ask.quantity).toLocaleString():''}</td></tr>`);
    }
    body.innerHTML=html.join('')||'<tr><td colspan="4" class="empty-cell">Book is empty at this snapshot.</td></tr>';
  }

  function updateMetrics(record) {
    const set=(id,value)=>{const node=$(`#${id}`);if(node)node.textContent=value;};
    set('metric-bid',fmt(record.best_bid));set('metric-ask',fmt(record.best_ask));set('metric-spread',fmt(record.spread));set('metric-mid',fmt(record.midpoint));
    set('metric-bid-depth',record.bid_depth==null?'—':Number(record.bid_depth).toLocaleString());
    set('metric-ask-depth',record.ask_depth==null?'—':Number(record.ask_depth).toLocaleString());
    set('metric-imbalance',record.imbalance==null?'—':fmt(record.imbalance,4));
    const pos=$('#snapshot-position');if(pos)pos.textContent=`Snapshot ${selected+1} / ${snapshots.length}`;
    const ts=$('#snapshot-timestamp');if(ts)ts.textContent=prettyTime(record.timestamp);
    const msg=$('#message-position');if(msg)msg.textContent=`Message ${record.message_index==null?'—':Number(record.message_index).toLocaleString()}`;
    const title=$('#replay-state-title');if(title)title.textContent=`Snapshot ${selected+1}`;
    const subtitle=$('#replay-state-subtitle');if(subtitle)subtitle.textContent=record.levels_available?'Per-price levels available. All replay views are synchronized.':'Aggregate observation only. Per-level views are unavailable.';
  }

  function drawAll() {
    const record=snapshots[selected];if(!record)return;
    const slider=$('#timeline-slider');if(slider)slider.value=String(selected);
    updateMetrics(record);drawDepth(record);drawLadder(record);drawHeatmap();
  }

  function colorFor(value,maxValue){
    if(value==null)return '#b9c1cc';
    if(value<=0)return '#f2f5f8';
    const t=Math.max(0,Math.min(1,Math.log1p(value)/Math.log1p(Math.max(1,maxValue))));
    const stops=[[0,244,248,250],[.35,143,220,205],[.7,45,143,188],[1,63,57,133]];
    let a=stops[0],b=stops[stops.length-1];
    for(let i=0;i<stops.length-1;i++){if(t>=stops[i][0]&&t<=stops[i+1][0]){a=stops[i];b=stops[i+1];break;}}
    const ratio=(t-a[0])/(b[0]-a[0]||1);
    const rgb=[1,2,3].map(index=>Math.round(a[index]+(b[index]-a[index])*ratio));
    return `rgb(${rgb[0]},${rgb[1]},${rgb[2]})`;
  }

  function drawHeatmap() {
    const canvas=$('#heatmap-canvas');const empty=$('#heatmap-unavailable');
    if(!canvas)return;
    if(!heatmap?.available){canvas.hidden=true;if(empty){empty.hidden=false;empty.textContent=heatmap?.reason||'Heatmap unavailable: per-level data was not produced.';}heatmapHits=null;return;}
    canvas.hidden=false;if(empty)empty.hidden=true;
    const sized=canvasContext(canvas);if(!sized)return;const ctx=sized.ctx,width=sized.width,height=sized.height;
    const left=54,right=14,top=13,bottom=31,plotW=Math.max(1,width-left-right),plotH=Math.max(1,height-top-bottom);
    const prices=heatmap.prices||[],frames=heatmap.frames||[],matrix=heatmap.values||[];
    if(!prices.length||!frames.length){drawText(ctx,'No level observations are available',width/2,height/2,'#8995a7','center',10);return;}
    const cellW=plotW/frames.length,cellH=plotH/prices.length;const maxVal=heatmap.quantity_max||1;
    heatmapHits={left,right,top,bottom,plotW,plotH,cellW,cellH,prices,frames,matrix};
    for(let row=0;row<prices.length;row++){
      const y=top+(prices.length-1-row)*cellH;
      for(let col=0;col<frames.length;col++){
        const v=matrix[row]?.[col]??null;ctx.fillStyle=colorFor(v,maxVal);ctx.fillRect(left+col*cellW,y,Math.ceil(cellW+.35),Math.ceil(cellH+.35));
      }
    }
    // Axis labels: price along y, selected message/time along x.
    ctx.strokeStyle='#dfe5ed';ctx.lineWidth=1;ctx.beginPath();ctx.moveTo(left,top);ctx.lineTo(left,top+plotH);ctx.lineTo(left+plotW,top+plotH);ctx.stroke();
    for(let i=0;i<Math.min(6,prices.length);i++){
      const idx=Math.round(i*(prices.length-1)/Math.max(1,Math.min(6,prices.length)-1));
      const reverse=prices.length-1-idx;const y=top+(prices.length-1-idx+.5)*cellH;
      drawText(ctx,fmt(prices[idx]),left-7,y+3,'#8894a7','right',8);
    }
    const labels=[0,Math.floor((frames.length-1)/2),frames.length-1];
    for(const idx of [...new Set(labels)]){
      const x=left+(idx+.5)*cellW;const label=frames[idx]?.message_index!=null?Number(frames[idx].message_index).toLocaleString():String(idx);
      drawText(ctx,label,x,height-11,'#8894a7','center',8);
    }
    // Prefer the nearest sampled heatmap frame for the playback cursor.
    let nearest=0,bestDist=Infinity;(heatmap.frame_indices||[]).forEach((value,i)=>{const d=Math.abs(value-selected);if(d<bestDist){bestDist=d;nearest=i;}});
    ctx.strokeStyle='#151f32';ctx.lineWidth=1.4;ctx.beginPath();ctx.moveTo(left+(nearest+.5)*cellW,top);ctx.lineTo(left+(nearest+.5)*cellW,top+plotH);ctx.stroke();
    const caption=$('#heatmap-caption');if(caption)caption.textContent=`${frames.length} sampled frames · ${prices.length} observed price levels · intensity uses displayed quantity`;
  }

  function showTooltip(node, event, html) {
    if(!node)return;const box=node.parentElement.getBoundingClientRect();node.innerHTML=html;node.hidden=false;
    const x=Math.max(6,Math.min(box.width-220,event.clientX-box.left+12));const y=Math.max(6,Math.min(box.height-80,event.clientY-box.top+12));node.style.left=`${x}px`;node.style.top=`${y}px`;
  }

  function initHover() {
    const depth=$('#depth-canvas'),depthTip=$('#depth-tooltip');
    depth?.addEventListener('mousemove',event=>{
      if(depth.hidden||!depthHits.length){if(depthTip)depthTip.hidden=true;return;}
      const rect=depth.getBoundingClientRect(),x=event.clientX-rect.left,y=event.clientY-rect.top;
      const hit=depthHits.find(item=>x>=item.x&&x<=item.x+item.w&&y>=item.y&&y<=item.y+item.h);
      if(!hit){if(depthTip)depthTip.hidden=true;return;}
      showTooltip(depthTip,event,`<strong>${hit.side.toUpperCase()}</strong><br>Price: ${fmt(hit.price)}<br>Level quantity: ${Number(hit.quantity).toLocaleString()}<br>Cumulative displayed depth: ${Number(hit.cumulative).toLocaleString()}`);
    });
    depth?.addEventListener('mouseleave',()=>{if(depthTip)depthTip.hidden=true;});

    const canvas=$('#heatmap-canvas'),tip=$('#heatmap-tooltip');
    canvas?.addEventListener('mousemove',event=>{
      if(canvas.hidden||!heatmapHits){if(tip)tip.hidden=true;return;}
      const rect=canvas.getBoundingClientRect(),x=event.clientX-rect.left-heatmapHits.left,y=event.clientY-rect.top-heatmapHits.top;
      if(x<0||y<0||x>heatmapHits.plotW||y>heatmapHits.plotH){if(tip)tip.hidden=true;return;}
      const col=Math.min(heatmapHits.frames.length-1,Math.floor(x/heatmapHits.cellW));
      const row=Math.max(0,Math.min(heatmapHits.prices.length-1,heatmapHits.prices.length-1-Math.floor(y/heatmapHits.cellH)));
      const frame=heatmapHits.frames[col],price=heatmapHits.prices[row],quantity=heatmapHits.matrix[row]?.[col]??null;
      const side=frame?.midpoint==null?'Side unavailable':price<=frame.midpoint?'Bid side':'Ask side';
      showTooltip(tip,event,`<strong>${side}</strong><br>Price: ${fmt(price)}<br>Quantity: ${quantity==null?'Unobserved':Number(quantity).toLocaleString()}<br>Timestamp: ${escapeHtml(frame?.timestamp??'—')}<br>Message: ${frame?.message_index==null?'—':Number(frame.message_index).toLocaleString()}`);
    });
    canvas?.addEventListener('mouseleave',()=>{if(tip)tip.hidden=true;});
    canvas?.addEventListener('click',event=>{
      if(!heatmapHits||!heatmap?.frame_indices?.length)return;
      const rect=canvas.getBoundingClientRect(),x=event.clientX-rect.left-heatmapHits.left;
      const col=Math.max(0,Math.min(heatmapHits.frames.length-1,Math.floor(x/heatmapHits.cellW)));
      const idx=heatmap.frame_indices[col];if(Number.isInteger(idx)&&idx>=0&&idx<snapshots.length){selected=idx;drawAll();}
    });
  }

  async function loadReplay(id) {
    stopPlayback();runId=id;snapshots=[];heatmap=null;selected=0;showReplayError('');setButtons(false);
    const title=$('#replay-state-title');if(title)title.textContent='Loading replay data…';
    try {
      if(!id){throw new Error('No completed run selected. Choose a run with a successful analysis status.');}
      const first=await window.LobDashboard.fetchJson(`/api/runs/${encodeURIComponent(id)}/snapshots?start=0&limit=5000`);
      snapshots=first.snapshots||[];
      for(let start=5000;start<first.total;start+=5000){
        const page=await window.LobDashboard.fetchJson(`/api/runs/${encodeURIComponent(id)}/snapshots?start=${start}&limit=5000`);
        snapshots.push(...(page.snapshots||[]));
      }
      const heatmapPromise=window.LobDashboard.fetchJson(`/api/runs/${encodeURIComponent(id)}/heatmap`).catch(error=>({available:false,reason:error.message}));
      heatmap=await heatmapPromise;
      const slider=$('#timeline-slider');if(slider){slider.min='0';slider.max=String(Math.max(0,snapshots.length-1));slider.value='0';}
      setButtons(snapshots.length>0);
      if(!snapshots.length)throw new Error('The run has no replay snapshots.');
      drawAll();
    } catch(error) {
      showReplayError(error.message);
      const title=$('#replay-state-title');if(title)title.textContent='Replay unavailable';
      const subtitle=$('#replay-state-subtitle');if(subtitle)subtitle.textContent='Complete a successful run before opening the replay page.';
      setButtons(false);
      const body=$('#ladder-body');if(body)body.innerHTML=`<tr><td colspan="4" class="empty-cell">${escapeHtml(error.message)}</td></tr>`;
      const depthEmpty=$('#depth-unavailable');if(depthEmpty){depthEmpty.hidden=false;depthEmpty.textContent=error.message;}
      const heatEmpty=$('#heatmap-unavailable');if(heatEmpty){heatEmpty.hidden=false;heatEmpty.textContent=error.message;}
    }
  }

  function init() {
    if(!$('#replay-run-select'))return;
    initHover();
    $('#replay-run-select')?.addEventListener('change',event=>loadReplay(event.target.value));
    $('#timeline-slider')?.addEventListener('input',event=>{stopPlayback();selected=Number(event.target.value);drawAll();});
    $('#play-btn')?.addEventListener('click',()=>{
      if(!snapshots.length||playTimer!==null)return;
      if(selected>=snapshots.length-1)selected=0;
      const delay=()=>Math.max(70,Number($('#speed-select')?.value||650));
      playTimer=setInterval(()=>{if(selected>=snapshots.length-1){stopPlayback();return;}selected++;drawAll();},delay());
    });
    $('#speed-select')?.addEventListener('change',()=>{
      if(playTimer===null)return;
      stopPlayback();
      const delay=Math.max(70,Number($('#speed-select')?.value||650));
      playTimer=setInterval(()=>{if(selected>=snapshots.length-1){stopPlayback();return;}selected++;drawAll();},delay);
    });
    $('#pause-btn')?.addEventListener('click',stopPlayback);
    $('#reset-btn')?.addEventListener('click',()=>{stopPlayback();selected=0;drawAll();});
    $('#prev-btn')?.addEventListener('click',()=>{stopPlayback();selected=Math.max(0,selected-1);drawAll();});
    $('#next-btn')?.addEventListener('click',()=>{stopPlayback();selected=Math.min(snapshots.length-1,selected+1);drawAll();});
    window.addEventListener('resize',()=>{if(snapshots[selected])drawAll();});
    const selector=$('#replay-run-select');
    // dashboard.js loads the run list asynchronously, so observe its option updates directly.
    const desired=new URLSearchParams(location.search).get('run_id');
    const choose=()=>{
      if(desired&&Array.from(selector.options).some(item=>item.value===desired))selector.value=desired;
      if(selector.value)loadReplay(selector.value);
    };
    choose();
    const observer=new MutationObserver(()=>{
      if(selector.options.length>1){choose();observer.disconnect();}
    });
    observer.observe(selector,{childList:true});
  }
  document.addEventListener('DOMContentLoaded',init);
})();

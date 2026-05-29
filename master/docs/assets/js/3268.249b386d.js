"use strict";(self.webpackChunksatoru_docs=self.webpackChunksatoru_docs||[]).push([["3268"],{6047(e,t,a){a.d(t,{diagram:()=>v});var i=a(8096),l=a(4909),r=a(7233),s=a(1745),n=a(863),o=a(2),d=a(2176),p=s.UI.pie,c={sections:new Map,showData:!1,config:p},u=c.sections,g=c.showData,h=structuredClone(p),f=(0,n.K2)(()=>structuredClone(h),"getConfig"),m=(0,n.K2)(()=>{u=new Map,g=c.showData,(0,s.IU)()},"clear"),x=(0,n.K2)(({label:e,value:t})=>{if(t<0)throw Error(`"${e}" has invalid value: ${t}. Negative values are not allowed in pie charts. All slice values must be >= 0.`);u.has(e)||(u.set(e,t),n.Rm.debug(`added new section: ${e}, with value: ${t}`))},"addSection"),w=(0,n.K2)(()=>u,"getSections"),$=(0,n.K2)(e=>{g=e},"setShowData"),S=(0,n.K2)(()=>g,"getShowData"),y={getConfig:f,clear:m,setDiagramTitle:s.ke,getDiagramTitle:s.ab,setAccTitle:s.SV,getAccTitle:s.iN,setAccDescription:s.EI,getAccDescription:s.m7,addSection:x,getSections:w,setShowData:$,getShowData:S},C=(0,n.K2)((e,t)=>{(0,l.S)(e,t),t.setShowData(e.showData),e.sections.map(t.addSection)},"populateDb"),T={parse:(0,n.K2)(async e=>{let t=await (0,o.qg)("pie",e);n.Rm.debug(t),C(t,y)},"parse")},k=(0,n.K2)(e=>`
  .pieCircle{
    stroke: ${e.pieStrokeColor};
    stroke-width : ${e.pieStrokeWidth};
    opacity : ${e.pieOpacity};
  }
  .pieOuterCircle{
    stroke: ${e.pieOuterStrokeColor};
    stroke-width: ${e.pieOuterStrokeWidth};
    fill: none;
  }
  .pieTitleText {
    text-anchor: middle;
    font-size: ${e.pieTitleTextSize};
    fill: ${e.pieTitleTextColor};
    font-family: ${e.fontFamily};
  }
  .slice {
    font-family: ${e.fontFamily};
    fill: ${e.pieSectionTextColor};
    font-size:${e.pieSectionTextSize};
    // fill: white;
  }
  .legend text {
    fill: ${e.pieLegendTextColor};
    font-family: ${e.fontFamily};
    font-size: ${e.pieLegendTextSize};
  }
`,"getStyles"),D=(0,n.K2)(e=>{let t=[...e.values()].reduce((e,t)=>e+t,0),a=[...e.entries()].map(([e,t])=>({label:e,value:t})).filter(e=>e.value/t*100>=1);return(0,d.rLf)().value(e=>e.value).sort(null)(a)},"createPieArcs"),v={parser:T,db:y,renderer:{draw:(0,n.K2)((e,t,a,l)=>{n.Rm.debug("rendering pie chart\n"+e);let o=l.db,p=(0,s.D7)(),c=(0,r.$t)(o.getConfig(),p.pie),u=(0,i.D)(t),g=u.append("g");g.attr("transform","translate(225,225)");let{themeVariables:h}=p,[f]=(0,r.I5)(h.pieOuterStrokeWidth);f??=2;let m=c.textPosition,x=(0,d.JLW)().innerRadius(0).outerRadius(185),w=(0,d.JLW)().innerRadius(185*m).outerRadius(185*m);g.append("circle").attr("cx",0).attr("cy",0).attr("r",185+f/2).attr("class","pieOuterCircle");let $=o.getSections(),S=D($),y=[h.pie1,h.pie2,h.pie3,h.pie4,h.pie5,h.pie6,h.pie7,h.pie8,h.pie9,h.pie10,h.pie11,h.pie12],C=0;$.forEach(e=>{C+=e});let T=S.filter(e=>"0"!==(e.data.value/C*100).toFixed(0)),k=(0,d.UMr)(y).domain([...$.keys()]);g.selectAll("mySlices").data(T).enter().append("path").attr("d",x).attr("fill",e=>k(e.data.label)).attr("class","pieCircle"),g.selectAll("mySlices").data(T).enter().append("text").text(e=>(e.data.value/C*100).toFixed(0)+"%").attr("transform",e=>"translate("+w.centroid(e)+")").style("text-anchor","middle").attr("class","slice");let v=g.append("text").text(o.getDiagramTitle()).attr("x",0).attr("y",-200).attr("class","pieTitleText"),b=[...$.entries()].map(([e,t])=>({label:e,value:t})),K=g.selectAll(".legend").data(b).enter().append("g").attr("class","legend").attr("transform",(e,t)=>"translate(216,"+(22*t-22*b.length/2)+")");K.append("rect").attr("width",18).attr("height",18).style("fill",e=>k(e.label)).style("stroke",e=>k(e.label)),K.append("text").attr("x",22).attr("y",14).text(e=>o.getShowData()?`${e.label} [${e.value}]`:e.label);let A=Math.max(...K.selectAll("text").nodes().map(e=>e?.getBoundingClientRect().width??0)),R=v.node()?.getBoundingClientRect().width??0,M=Math.min(0,225-R/2),z=Math.max(512+A,225+R/2)-M;u.attr("viewBox",`${M} 0 ${z} 450`),(0,s.a$)(u,450,z,c.useMaxWidth)},"draw")},styles:k}}}]);
const char UMIDADE_HTML[] = 
"<!DOCTYPE html>"
"<html lang=\"pt-BR\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>Umidade</title><link href=\"https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css\" rel=\"stylesheet\"><script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script><style>body{background:#f5f7fa}.navbar{margin-bottom:1rem}canvas{width:100%!important;height:auto!important}</style></head><body>"
"<nav class=\"navbar navbar-expand-lg navbar-light bg-white shadow-sm\"><div class=\"container-fluid\"><a class=\"navbar-brand\" href=\"#\">MeteoStation</a><ul class=\"navbar-nav\"><li class=\"nav-item\"><a class=\"nav-link\" href=\"\\\">Dashboard</a></li><li class=\"nav-item\"><a class=\"nav-link\" href=\"\\temperatura\">Temperatura</a></li><li class=\"nav-item\"><a class=\"nav-link active\" href=\"\\umidade\">Umidade</a></li><li class=\"nav-item\"><a class=\"nav-link\" href=\"\\pressao\">Pressão</a></li><li class=\"nav-item dropdown\"><a class=\"nav-link dropdown-toggle\" data-bs-toggle=\"dropdown\" href=\"#\">Configurações</a><ul class=\"dropdown-menu\"><li><a class=\"dropdown-item\" href=\"\\limites\">Limites</a></li><li><a class=\"dropdown-item\" href=\"\\offset\">Offsets</a></li></ul></li></ul></div></nav>"
"<div class=\"container\"><canvas id=\"chart-hum\"></canvas></div>"
"<script>"
"async function fetchData(){let r=await fetch('/data');return r.ok?r.json():null;}"
"const L2=[],H2=[],M2=30,ctx2=document.getElementById('chart-hum').getContext('2d');"
"const C2=new Chart(ctx2,{type:'line',data:{labels:L2,datasets:[{label:'Umidade',data:H2,tension:.4}]},options:{responsive:!0}});"
"async function u2(){let d=await fetchData();if(!d)return;let t=new Date().toLocaleTimeString();L2.push(t);H2.push(d.humidity);if(L2.length>M2){L2.shift();H2.shift()}C2.update()}"
"window.addEventListener('load',()=>{u2();setInterval(u2,2000)});"
"</script>"
"<script src=\"https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/js/bootstrap.bundle.min.js\"></script>"
"</body></html>";

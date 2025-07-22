
const char TEMPERATURA_HTML[] = 
"<!DOCTYPE html>"
"<html lang=\"pt-BR\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>Temperatura</title><link href=\"https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css\" rel=\"stylesheet\"><script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script><style>body{background:#f5f7fa}.navbar{margin-bottom:1rem}canvas{width:100%!important;height:auto!important}</style></head><body>"
"<nav class=\"navbar navbar-expand-lg navbar-light bg-white shadow-sm\"><div class=\"container-fluid\"><a class=\"navbar-brand\" href=\"#\">MeteoStation</a><ul class=\"navbar-nav\"><li class=\"nav-item\"><a class=\"nav-link\" href=\"\\\">Dashboard</a></li><li class=\"nav-item\"><a class=\"nav-link active\" href=\"\\temperatura\">Temperatura</a></li><li class=\"nav-item\"><a class=\"nav-link\" href=\"\\umidade\">Umidade</a></li><li class=\"nav-item\"><a class=\"nav-link\" href=\"\\pressao\">Pressão</a></li><li class=\"nav-item dropdown\"><a class=\"nav-link dropdown-toggle\" data-bs-toggle=\"dropdown\" href=\"#\">Configurações</a><ul class=\"dropdown-menu\"><li><a class=\"dropdown-item\" href=\"\\limites\">Limites</a></li><li><a class=\"dropdown-item\" href=\"\\offset\">Offsets</a></li></ul></li></ul></div></nav>"
"<div class=\"container\"><canvas id=\"chart-temp\"></canvas></div>"
"<script>"
"async function fetchData(){let r=await fetch('/data');return r.ok?r.json():null;}"
"const L=[],A=[],B=[],M=30,ctx=document.getElementById('chart-temp').getContext('2d');"
"const C=new Chart(ctx,{type:'line',data:{labels:L,datasets:[{label:'AHT20',data:A,tension:.4},{label:'BMP280',data:B,tension:.4}]},options:{responsive:!0}});"
"async function u(){let d=await fetchData();if(!d)return;let t=new Date().toLocaleTimeString();L.push(t);A.push(d.temperature_aht20);B.push(d.temperature_bmp);if(L.length>M){L.shift();A.shift();B.shift()}C.update()}"
"window.addEventListener('load',()=>{u();setInterval(u,2000)});"
"</script>"
"<script src=\"https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/js/bootstrap.bundle.min.js\"></script>"
"</body></html>"
;

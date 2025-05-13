#ifndef HTML_DATA_H
#define HTML_DATA_H

const char *html_data =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html\r\n"
/* "Content-Length: 2220\r\n" // Substitua 1234 pelo tamanho real do conteúdo HTML
"Connection: close\r\n" */
"\r\n"
"<!DOCTYPE html><html lang=\"pt-br\"><head><meta charset=\"UTF-8\"><title>Estacionamento Inteligente</title>"
"<style>body{font-family:Arial,sans-serif;background:#f4f4f4;margin:0;padding:0;}.container{max-width:600px;margin:20px auto;padding:10px;background:#fff;border-radius:6px;box-shadow:0 2px 6px #0001;}h1{text-align:center;color:#222;}.content{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:20px;}.box{background:#e9e9e9;padding:10px;border-radius:4px;text-align:center;}.pcd{border:2px dashed #2196F3;background:#E3F2FD;position:relative;}.vaga-tipo{font-weight:bold;margin-bottom:6px;}.btn-reservar{background:#4CAF50;color:#fff;padding:6px 12px;border:none;border-radius:3px;cursor:pointer;margin-top:8px;}.btn-reservar:disabled{background:#bbb;cursor:not-allowed;}.status-indicator{width:14px;height:14px;border-radius:50%%;display:inline-block;margin-bottom:6px;}.disponivel{background:#4CAF50;}.ocupada{background:#f44336;}.reservada{background:#ff9800;}.status-text{font-size:0.9em;color:#555;margin:4px 0;}</style></head><body>"
"<div class=\"container\"><h1>Estacionamento Inteligente</h1><div class=\"content\">"
"<div class=\"box %s\"><div class=\"status-indicator %s\"></div><div class=\"vaga-tipo\">Vaga 1%s</div><p>%s</p><p class=\"status-text\">%s</p><form action=\"./reservar-vaga-1\"><button class=\"btn-reservar\" %s>Reservar</button></form></div>"
"<div class=\"box %s\"><div class=\"status-indicator %s\"></div><div class=\"vaga-tipo\">Vaga 2%s</div><p>%s</p><p class=\"status-text\">%s</p><form action=\"./reservar-vaga-2\"><button class=\"btn-reservar\" %s>Reservar</button></form></div>"
"<div class=\"box %s\"><div class=\"status-indicator %s\"></div><div class=\"vaga-tipo\">Vaga 3%s</div><p>%s</p><p class=\"status-text\">%s</p><form action=\"./reservar-vaga-3\"><button class=\"btn-reservar\" %s>Reservar</button></form></div>"
"<div class=\"box %s\"><div class=\"status-indicator %s\"></div><div class=\"vaga-tipo\">Vaga 4%s</div><p>%s</p><p class=\"status-text\">%s</p><form action=\"./reservar-vaga-4\"><button class=\"btn-reservar\" %s>Reservar</button></form></div>"
"</div></div><script>setTimeout(()=>{window.location.href = '/';},5000);</script></body></html>";

#endif // HTML_DATA_H

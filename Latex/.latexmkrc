# PDF junto a main.tex; archivos auxiliares en build/ (ignorado por git).
$pdf_mode = 1;
$aux_dir  = 'build';
$out_dir  = '.';
$pdflatex = 'pdflatex -interaction=nonstopmode -halt-on-error -file-line-error %O %S';
@default_files = ('main.tex');

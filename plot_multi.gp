set datafile separator whitespace
set term pngcairo size 1280,720
set output "wrat_T_overlay.png"
set grid
set key outside
set xlabel "Thickness (W)"
set ylabel "Transmission fraction"
set yrange [0:1]

S = "WRAT_serial.dat"
PAR = "1 2 4 8 12 16"

plot S using 1:($4/($2+$3+$4)) w lp title "Serial T", \
     for [p in PAR] sprintf("WRAT_parallel_p%s.dat", p) \
         using 1:($4/($2+$3+$4)) w lp title sprintf("Parallel T p=%s", p)

if x < 1 then
  while x < 1 do
    x := x + 1;
    y := y + x;
  end;
else
  x := 1;
  spawn task0; task1; task2; end;
  a := 2*x/(y + 2);
end
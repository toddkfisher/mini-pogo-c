module collatz;
  init
    x := 21;
    while x > 1 do
      print_int x;
      if x % 2 then
        x := 3*x + 1;
      else
        x := x/2;
      end; ! if
    end; ! while
  end; ! init
end; ! module

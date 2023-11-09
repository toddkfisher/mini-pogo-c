module fib;
  init
    print_int 999;
    spawn fibtask; end;
  end;

  task fibtask;
    a := 1;
    b := 1;
    print_int a;
    print_int b;
    while c < 10000 do
      c := a + b;
      print_int c;
      a := b;
      b := c;
    end;
  end;
end

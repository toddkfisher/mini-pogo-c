module fib;
  init
    a := 1;  ! a = Fn-2
    b := 1;  ! b = Fn-1
    print_int a;
    print_int b;
    c := a + b; ! c = Fn
    while c < 10000 do
      print_int c;
      a := b;
      b := c;
      c := a + b; ! c = Fn
    end;
  end;
end

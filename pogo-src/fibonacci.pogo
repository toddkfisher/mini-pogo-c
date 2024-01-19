module fib;
  init
    ! Spawn a task which will print fibonacci numbers < 10000.
    a := 1;  ! a = Fn-2
    b := 1;  ! b = Fn-1
    print_int a;
    print_int b;
    while c < 10000 do
      c := a + b; ! c = Fn
      print_int c;
      a := b;
      b := c;
    end;
  end;
end

module test
  init
    if a>1 or b=3 and c<b then
      print_int 123;
    else
      print_int 666;
    end;
    x := 1 + x;
    y := 6/2*(2+1);
    spawn t0; t1; t1; end;
  end;

  task t0;
    stop;
  end;

  task t1;
    if 0 then
      stop;
      stop;
    end;
  end;
end;
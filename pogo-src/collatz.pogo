module collatz;
  init
     x := 21;
     while x > 1 do
       print_int x;
       sleep 5;
       if x % 2 then
         x := 3*x + 1;
       else
         x := x/2;
       end;
     end;
    ! spawn
    !   t0;
    !   t1;
    !   t2;
    ! end;
  end;

  task t0
    while 1 do
      print_int 0;
      sleep 5; ! seconds
    end;
  end;

  task t1
    while 1 do
      print_int 1;
      sleep 5; ! seconds
    end;
  end;

  task t2
    while 1 do
      print_int 2;
      sleep 5; ! seconds
    end;
  end;
end;

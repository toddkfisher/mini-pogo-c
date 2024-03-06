module collatz
  init
     ! x := 21;
     ! while x > 1 do
     !   print_int x;
     !   sleep 5;
     !   if x % 2 then
     !     x := 3*x + 1;
     !   else
     !     x := x/2;
     !   end;
     ! end;
    spawn
      t0;
      t1;
      t2;
    join
    wait sec(5)
    timeout
      print "Join timed out in collatz init block.\n";
    else
      print "Join successful in collatz init block.\n";
    end;
  end;

  task t0
    while x < 4 do
      print_int 0;
      sleep sec(2);
      x := x + 1;
    end;
  end;

  task t1
    while x < 4 do
      print_int 1;
      sleep sec(2);
      x := x + 1;
    end;
  end;

  task t2
    while x < 4 do
      print_int 2;
      sleep sec(2);
      x := x + 1;
    end;
  end;
end;

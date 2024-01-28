module task_test;
  init
    spawn
      t0;
      t1;
      t2;
    end;  ! join here
  end;

  task t0
    while x < 5 do
      print_int 0;
      sleep 1; ! seconds
      x := x + 1;
    end;
  end;

  task t1
    while x < 5 do
      print_int 1;
      sleep 1; ! seconds
      x := x + 1;
    end;
  end;

  task t2
    while x < 5 do
      print_int 2;
      sleep 1; ! seconds
      x := x + 1;
    end;
  end;
end;

module test_module;
  init
    spawn a_random_task; end;
  end

  task a_random_task;
    if x < 1 then
      while x < 1 do
        x := x + 1;
        y := y + x;
        print_char '\n';
      end;
    else
      x := 1;
      spawn task0; task1; task2; end;
      a := 2*x/(y + 2);
    end;
  end

  task someOtherTask
     z := 666;
  end;

 end

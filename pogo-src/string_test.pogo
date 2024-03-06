module string_test;

  ! Test printing.
  init
    print "ab\"acde";
    print "one", a+b/666;
    print "two", 1;
    print "one", 2;   ! The string "one"  should only appear once  in the string
                      ! table
    print "one", 3;
    print "three", 4;
    print "four", 5;
    print "five\n", 6;
    print_char 'y';
    x := 123 + y;
  end;


  task t0;
    stop;
  end;


  task t1;
    while x < 10 do
      print "x and its square: ", x, x*x, "\n";
      x := x + 1;
    end;
  end;
end;
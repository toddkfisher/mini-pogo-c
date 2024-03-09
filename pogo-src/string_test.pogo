module string_test;

  ! Test printing.
  init
    ! print "ab\"acde";
    ! print "one", 666,777, "\n";
    ! print "two", 1, "\n";
    ! print "one", 2, "\n";   ! The string "one"  should only appear once  in the string
    !                         ! table
    ! print "one", 3, "\n";
    ! print "three", 4, "\n";
    ! print "four", 5, "\n";
    ! print "five\n", 6, "\n";
    x := 123 + y;
    if x < 10 then
    else
      print "blah\n";
    end;
  end;


!  task t0;
!    stop;
!  end;
!
!
!  task t1;
!    while x < 10 do
!      print "x and its square: ", x, x*x, "\n";
!      x := x + 1;
!    end;
!  end;
end;
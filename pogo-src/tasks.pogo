module tasks;
  init
  t := 1000*10; ! ten seconds
    print "Spawing t0, t1, t2..\n";
    spawn t0;t1;t2;
    join
    wait t
    timeout
      print "Join timed out.\n";
    else
      print "Joined in ten seconds (+/-) or less.\n";
    end;
  end;
  !-----------------------------------------------------------------------------
  task t0;
    print "Started t0.\n";
    sleep 3000;
    print "Ending t0.\n";
  end;
  !-----------------------------------------------------------------------------
  task t1;
    print "Started t1.\n";
    sleep 1000;
    print "Ending t1.\n";
  end;
  !-----------------------------------------------------------------------------
  task t2;
    print "Started t2.\n";
    sleep 150;
    print "Ending t2.\n";
  end;
end;
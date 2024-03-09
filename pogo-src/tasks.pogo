module tasks;
  init
    print "Spawing t0, t1, t2..\n";
    spawn t0;
    join;
    !wait 1000*10 ! ten seconds
    !timeout
    !  print "Join timed out.\n";
    !else
    !  print "Joined in one minute or less.\n";
    !end;
    print "Joined.\n";
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
    sleep 15000;
    print "Ending t1.\n";
  end;
  !-----------------------------------------------------------------------------
  task t2;
    print "Started t2.\n";
    sleep 150;
    print "Ending t2.\n";
  end;
end;
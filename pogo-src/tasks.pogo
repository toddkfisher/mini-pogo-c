module tasks;


  init
    spawn t0; t1; t2;
    join
    wait 1000*60 ! one minute
    timeout
      print "Join timed out.\n";
    else
      print "Joined in one minute or less.\n";
    end;
  end;


  task t0;
    print "Started t0.\n";
    sleep 15;
    print "Ending t0.\n";
  end;


  task t1;
    print "Started t1.\n";
    sleep 15;
    print "Ending t1.\n";
  end;


  task t2;
    print "Started t2.\n";
    sleep 15;
    print "Ending t2.\n";
  end;
end;
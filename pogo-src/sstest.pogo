module shortcircuittest;
  init
    x := 1;
    if 0 and x/0 then
      print_char 'x';
      print_char '\n';
    else
      if 1 or x/0 then
        print_char 'y';
        print_char '\n';
      else
        print_char 'w';
        print_char '\n';
      end;
    end;
  end;
end;

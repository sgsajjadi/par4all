

void simpleExampleDynamicLoadedFunction();

int main() {
  // This function is not known by pips at workspace creation and will be loaded on the fly
  simpleExampleDynamicLoadedFunction();
}

